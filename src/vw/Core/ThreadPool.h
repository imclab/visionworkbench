// __BEGIN_LICENSE__
//  Copyright (c) 2006-2013, United States Government as represented by the
//  Administrator of the National Aeronautics and Space Administration. All
//  rights reserved.
//
//  The NASA Vision Workbench is licensed under the Apache License,
//  Version 2.0 (the "License"); you may not use this file except in
//  compliance with the License. You may obtain a copy of the License at
//  http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
// __END_LICENSE__


/// \file vw/Core/ThreadPool.h
///
/// Note: All tasks need to be of the same type, but you can have a
/// common abstract base class if you want.
///
#ifndef __VW_CORE_THREADPOOL_H__
#define __VW_CORE_THREADPOOL_H__

#include <vector>
#include <list>

#include <vw/Core/Condition.h>
#include <vw/Core/Settings.h>
#include <vw/Core/Thread.h>

// STL
#include <map>

namespace vw {

  // ----------------------  --------------  ---------------------------
  // ----------------------       Task       ---------------------------
  // ----------------------  --------------  ---------------------------
  class Task {
    Mutex m_task_mutex;
    Condition m_finished_event;
    volatile bool m_finished;

  public:
    Task();
    virtual ~Task();
    virtual void operator()() = 0;

    // Keep track of whether task is finished. WorkQueue is responsible
    // for calling "signal_finished" after task is finished
    bool is_finished();
    void join();
    void signal_finished();
  };

  // ----------------------  --------------  ---------------------------
  // ----------------------  Task Generator  ---------------------------
  // ----------------------  --------------  ---------------------------

  // Work Queue Base Class
  class WorkQueue {

    // The worker thread class is the Task object that is spun out to
    // do the actual work of the WorkQueue.  When a worker thread
    // finishes its task it notifies the threadpool, which farms out
    // the next task to the worker thread.
    class WorkerThread {
      WorkQueue &m_queue;
      boost::shared_ptr<Task> m_task;
      int m_thread_id;
      bool &m_should_die;
    public:
      WorkerThread(WorkQueue& queue, boost::shared_ptr<Task> initial_task,
                   int thread_id, bool &should_die);
      ~WorkerThread();
      void operator()();
    };

    int m_active_workers, m_max_workers;
    Mutex m_queue_mutex;
    std::vector<boost::shared_ptr<Thread> > m_running_threads;
    std::list<int> m_available_thread_ids;
    Condition m_joined_event;
    bool m_should_die;

    // This is called whenever a worker thread finishes its task. If
    // there are more tasks available, the worker is given more work.
    // Otherwise, the worker terminates.
    //
    // *************************************************************
    // IMPORANT NOTE: The worker_thread_complete() method is called
    // from within child thread so that we can clean up the list of
    // available threads.  As such, one must be very careful about
    // what is done in this method, because for the duration of this
    // method, the child thread has access to it's own shared pointer.
    // This method has been carefully written so as not to
    // accidentally allow the thread to delete it's own shared
    // pointer, which could cause the thread to call its own join()
    // method.
    // *************************************************************
    void worker_thread_complete(int worker_id);

  public:
    WorkQueue(int num_threads = vw_settings().default_num_threads() );
    virtual ~WorkQueue();

    /// Return a shared pointer to the next task.  If no tasks are
    /// available, return an empty shared pointer.
    virtual boost::shared_ptr<Task> get_next_task() = 0;

    // Notify can be called by a child class that inherits from
    // WorkQueue.  A call to notify will cause the WorkQueue to
    // re-examine the list of tasks it has available for execution.
    // If there are any idle slots for worker threads, it will spin
    // off WorkerThreads to execute these tasks.
    void notify();

    /// Return the max number threads that can run concurrently at any
    /// given time using this threadpool.
    int max_threads();

    /// Return the max number threads that can run concurrently at any
    /// given time using this threadpool.
    int active_threads();

    // Join all currently running threads and wait for the task pool
    // to be empty.
    void join_all();
    void kill_and_join();
  };



  /// A simple, first-in, first-out work queue.
  class FifoWorkQueue : public WorkQueue {
    std::list<boost::shared_ptr<Task> > m_queued_tasks;
    Mutex m_mutex;
  public:

    FifoWorkQueue(int num_threads = vw_settings().default_num_threads());

    size_t size();

    // Add a task that is being tracked by a shared pointer.
    void add_task(boost::shared_ptr<Task> task);

    virtual boost::shared_ptr<Task> get_next_task();
  };

  /// A simple ordered work queue.  Tasks are each given an "index"
  /// and they are processed in order starting with the task at index
  /// 0.  The idle() method returns true unless the task with the next
  /// expected index is present in the work queue.
  class OrderedWorkQueue : public WorkQueue {
    std::map<int, boost::shared_ptr<Task> > m_queued_tasks;
    int m_next_index;
    Mutex m_mutex;
  public:

    OrderedWorkQueue(int num_threads = vw_settings().default_num_threads());

    size_t size();

    // Add a task that is being tracked by a shared pointer.
    void add_task(boost::shared_ptr<Task> task, int index);

    virtual boost::shared_ptr<Task> get_next_task();
  };

} // namespace vw

#endif // __VW_CORE_THREADPOOL_H__

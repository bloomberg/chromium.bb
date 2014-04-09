// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_RASTER_WORKER_POOL_H_
#define CC_RESOURCES_RASTER_WORKER_POOL_H_

#include <vector>

#include "base/callback.h"
#include "cc/resources/resource_format.h"
#include "cc/resources/task_graph_runner.h"

class SkCanvas;

namespace base {
class SequencedTaskRunner;
}

namespace cc {
class Resource;

namespace internal {
class WorkerPoolTask;

class CC_EXPORT WorkerPoolTaskClient {
 public:
  virtual SkCanvas* AcquireCanvasForRaster(WorkerPoolTask* task,
                                           const Resource* resource) = 0;
  virtual void ReleaseCanvasForRaster(WorkerPoolTask* task,
                                      const Resource* resource) = 0;

 protected:
  virtual ~WorkerPoolTaskClient() {}
};

class CC_EXPORT WorkerPoolTask : public Task {
 public:
  typedef std::vector<scoped_refptr<WorkerPoolTask> > Vector;

  virtual void ScheduleOnOriginThread(WorkerPoolTaskClient* client) = 0;
  virtual void RunOnOriginThread() = 0;
  virtual void CompleteOnOriginThread(WorkerPoolTaskClient* client) = 0;
  virtual void RunReplyOnOriginThread() = 0;

  void WillSchedule();
  void DidSchedule();
  bool HasBeenScheduled() const;

  void WillComplete();
  void DidComplete();
  bool HasCompleted() const;

 protected:
  WorkerPoolTask();
  virtual ~WorkerPoolTask();

  bool did_schedule_;
  bool did_complete_;
};

class CC_EXPORT RasterWorkerPoolTask : public WorkerPoolTask {
 public:
  const Resource* resource() const { return resource_; }
  const internal::WorkerPoolTask::Vector& dependencies() const {
    return dependencies_;
  }

 protected:
  RasterWorkerPoolTask(const Resource* resource,
                       internal::WorkerPoolTask::Vector* dependencies);
  virtual ~RasterWorkerPoolTask();

 private:
  const Resource* resource_;
  WorkerPoolTask::Vector dependencies_;
};

}  // namespace internal

class CC_EXPORT RasterWorkerPoolClient {
 public:
  virtual bool ShouldForceTasksRequiredForActivationToComplete() const = 0;
  virtual void DidFinishRunningTasks() = 0;
  virtual void DidFinishRunningTasksRequiredForActivation() = 0;

 protected:
  virtual ~RasterWorkerPoolClient() {}
};

struct CC_EXPORT RasterTaskQueue {
  struct CC_EXPORT Item {
    class TaskComparator {
     public:
      explicit TaskComparator(const internal::WorkerPoolTask* task)
          : task_(task) {}

      bool operator()(const Item& item) const { return item.task == task_; }

     private:
      const internal::WorkerPoolTask* task_;
    };

    typedef std::vector<Item> Vector;

    Item(internal::RasterWorkerPoolTask* task, bool required_for_activation);
    ~Item();

    static bool IsRequiredForActivation(const Item& item) {
      return item.required_for_activation;
    }

    internal::RasterWorkerPoolTask* task;
    bool required_for_activation;
  };

  RasterTaskQueue();
  ~RasterTaskQueue();

  void Swap(RasterTaskQueue* other);
  void Reset();

  Item::Vector items;
  size_t required_for_activation_count;
};

// This interface can be used to schedule and run raster tasks. The client will
// be notified asynchronously when the set of tasks marked as "required for
// activation" have finished running and when all scheduled tasks have finished
// running. The client can call CheckForCompletedTasks() at any time to dispatch
// pending completion callbacks for all tasks that have finished running.
class CC_EXPORT RasterWorkerPool {
 public:
  static unsigned kOnDemandRasterTaskPriority;
  static unsigned kBenchmarkRasterTaskPriority;
  static unsigned kRasterFinishedTaskPriority;
  static unsigned kRasterRequiredForActivationFinishedTaskPriority;
  static unsigned kRasterTaskPriorityBase;

  // Set the number of threads to use for the global TaskGraphRunner instance.
  // This can only be called once and must be called prior to
  // GetNumRasterThreads().
  static void SetNumRasterThreads(int num_threads);

  // Returns the number of threads used for the global TaskGraphRunner instance.
  static int GetNumRasterThreads();

  // Returns a pointer to the global TaskGraphRunner instance.
  static internal::TaskGraphRunner* GetTaskGraphRunner();

  // Returns a unique clone index for the current thread. Guaranteed to be a
  // value between 0 and GetNumRasterThreads() - 1.
  static size_t GetPictureCloneIndexForCurrentThread();

  // Utility function that can be used by implementations to create a "raster
  // finished" task that posts |callback| to |task_runner| when run.
  static scoped_refptr<internal::WorkerPoolTask> CreateRasterFinishedTask(
      base::SequencedTaskRunner* task_runner,
      const base::Closure& callback);

  // Utility function that can be used by implementations to create a "raster
  // required for activation finished" task that posts |callback| to
  // |task_runner| when run.
  static scoped_refptr<internal::WorkerPoolTask>
      CreateRasterRequiredForActivationFinishedTask(
          size_t tasks_required_for_activation_count,
          base::SequencedTaskRunner* task_runner,
          const base::Closure& callback);

  // Utility function that can be used by implementations to call
  // ::ScheduleOnOriginThread() for each task in |graph|.
  static void ScheduleTasksOnOriginThread(
      internal::WorkerPoolTaskClient* client,
      internal::TaskGraph* graph);

  // Utility function that can be used by implementations to build a task graph.
  // Inserts a node that represents |task| in |graph|. See TaskGraph definition
  // for valid |priority| values.
  static void InsertNodeForTask(internal::TaskGraph* graph,
                                internal::WorkerPoolTask* task,
                                unsigned priority,
                                size_t dependencies);

  // Utility function that can be used by implementations to build a task graph.
  // Inserts nodes that represent |task| and all its image decode dependencies
  // in |graph|.
  static void InsertNodesForRasterTask(
      internal::TaskGraph* graph,
      internal::WorkerPoolTask* task,
      const internal::WorkerPoolTask::Vector& decode_tasks,
      unsigned priority);

  // Set the client instance to be notified when finished running tasks.
  virtual void SetClient(RasterWorkerPoolClient* client) = 0;

  // Tells the worker pool to shutdown after canceling all previously scheduled
  // tasks. Reply callbacks are still guaranteed to run when
  // CheckForCompletedTasks() is called.
  virtual void Shutdown() = 0;

  // Schedule running of raster tasks in |queue| and all dependencies.
  // Previously scheduled tasks that are not in |queue| will be canceled unless
  // already running. Once scheduled, reply callbacks are guaranteed to run for
  // all tasks even if they later get canceled by another call to
  // ScheduleTasks().
  virtual void ScheduleTasks(RasterTaskQueue* queue) = 0;

  // Check for completed tasks and dispatch reply callbacks.
  virtual void CheckForCompletedTasks() = 0;

  // Returns the target that needs to be used for raster task resources.
  virtual unsigned GetResourceTarget() const = 0;

  // Returns the format that needs to be used for raster task resources.
  virtual ResourceFormat GetResourceFormat() const = 0;

 protected:
  virtual ~RasterWorkerPool() {}
};

}  // namespace cc

#endif  // CC_RESOURCES_RASTER_WORKER_POOL_H_

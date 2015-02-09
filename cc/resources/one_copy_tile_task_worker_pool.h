// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_ONE_COPY_TILE_TASK_WORKER_POOL_H_
#define CC_RESOURCES_ONE_COPY_TILE_TASK_WORKER_POOL_H_

#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "cc/base/scoped_ptr_deque.h"
#include "cc/output/context_provider.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/tile_task_runner.h"
#include "cc/resources/tile_task_worker_pool.h"

namespace base {
namespace trace_event {
class ConvertableToTraceFormat;
class TracedValue;
}
}

namespace cc {
class ResourcePool;
class ScopedResource;

typedef int64 CopySequenceNumber;

class CC_EXPORT OneCopyTileTaskWorkerPool : public TileTaskWorkerPool,
                                            public TileTaskRunner,
                                            public TileTaskClient {
 public:
  ~OneCopyTileTaskWorkerPool() override;

  static scoped_ptr<TileTaskWorkerPool> Create(
      base::SequencedTaskRunner* task_runner,
      TaskGraphRunner* task_graph_runner,
      ContextProvider* context_provider,
      ResourceProvider* resource_provider,
      ResourcePool* resource_pool);

  // Overridden from TileTaskWorkerPool:
  TileTaskRunner* AsTileTaskRunner() override;

  // Overridden from TileTaskRunner:
  void SetClient(TileTaskRunnerClient* client) override;
  void Shutdown() override;
  void ScheduleTasks(TileTaskQueue* queue) override;
  void CheckForCompletedTasks() override;
  ResourceFormat GetResourceFormat() override;

  // Overridden from TileTaskClient:
  scoped_ptr<RasterBuffer> AcquireBufferForRaster(
      const Resource* resource) override;
  void ReleaseBufferForRaster(scoped_ptr<RasterBuffer> buffer) override;

  // Playback raster source and schedule copy of |src| resource to |dst|
  // resource. Returns a non-zero sequence number for this copy operation.
  CopySequenceNumber PlaybackAndScheduleCopyOnWorkerThread(
      scoped_ptr<ResourceProvider::ScopedWriteLockGpuMemoryBuffer> write_lock,
      scoped_ptr<ScopedResource> src,
      const Resource* dst,
      const RasterSource* raster_source,
      const gfx::Rect& rect,
      float scale);

  // Issues copy operations until |sequence| has been processed. This will
  // return immediately if |sequence| has already been processed.
  void AdvanceLastIssuedCopyTo(CopySequenceNumber sequence);

 protected:
  OneCopyTileTaskWorkerPool(base::SequencedTaskRunner* task_runner,
                            TaskGraphRunner* task_graph_runner,
                            ContextProvider* context_provider,
                            ResourceProvider* resource_provider,
                            ResourcePool* resource_pool);

 private:
  struct CopyOperation {
    typedef ScopedPtrDeque<CopyOperation> Deque;

    CopyOperation(
        scoped_ptr<ResourceProvider::ScopedWriteLockGpuMemoryBuffer> write_lock,
        scoped_ptr<ScopedResource> src,
        const Resource* dst);
    ~CopyOperation();

    scoped_ptr<ResourceProvider::ScopedWriteLockGpuMemoryBuffer> write_lock;
    scoped_ptr<ScopedResource> src;
    const Resource* dst;
  };

  void OnTaskSetFinished(TaskSet task_set);
  void AdvanceLastFlushedCopyTo(CopySequenceNumber sequence);
  void IssueCopyOperations(int64 count);
  void ScheduleCheckForCompletedCopyOperationsWithLockAcquired(
      bool wait_if_needed);
  void CheckForCompletedCopyOperations(bool wait_if_needed);
  scoped_refptr<base::trace_event::ConvertableToTraceFormat> StateAsValue()
      const;
  void StagingStateAsValueInto(
      base::trace_event::TracedValue* staging_state) const;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  TaskGraphRunner* task_graph_runner_;
  const NamespaceToken namespace_token_;
  TileTaskRunnerClient* client_;
  ContextProvider* context_provider_;
  ResourceProvider* resource_provider_;
  ResourcePool* resource_pool_;
  TaskSetCollection tasks_pending_;
  scoped_refptr<TileTask> task_set_finished_tasks_[kNumberOfTaskSets];
  CopySequenceNumber last_issued_copy_operation_;
  CopySequenceNumber last_flushed_copy_operation_;

  // Task graph used when scheduling tasks and vector used to gather
  // completed tasks.
  TaskGraph graph_;
  Task::Vector completed_tasks_;

  base::Lock lock_;
  // |lock_| must be acquired when accessing the following members.
  base::ConditionVariable copy_operation_count_cv_;
  size_t scheduled_copy_operation_count_;
  size_t issued_copy_operation_count_;
  CopyOperation::Deque pending_copy_operations_;
  CopySequenceNumber next_copy_operation_sequence_;
  bool check_for_completed_copy_operations_pending_;
  base::TimeTicks last_check_for_completed_copy_operations_time_;
  bool shutdown_;

  base::WeakPtrFactory<OneCopyTileTaskWorkerPool> weak_ptr_factory_;
  // "raster finished" tasks need their own factory as they need to be
  // canceled when ScheduleTasks() is called.
  base::WeakPtrFactory<OneCopyTileTaskWorkerPool>
      task_set_finished_weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OneCopyTileTaskWorkerPool);
};

}  // namespace cc

#endif  // CC_RESOURCES_ONE_COPY_TILE_TASK_WORKER_POOL_H_

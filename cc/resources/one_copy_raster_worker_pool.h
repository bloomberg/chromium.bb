// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_ONE_COPY_RASTER_WORKER_POOL_H_
#define CC_RESOURCES_ONE_COPY_RASTER_WORKER_POOL_H_

#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "cc/base/scoped_ptr_deque.h"
#include "cc/output/context_provider.h"
#include "cc/resources/raster_worker_pool.h"
#include "cc/resources/rasterizer.h"
#include "cc/resources/resource_provider.h"

namespace base {
namespace debug {
class ConvertableToTraceFormat;
class TracedValue;
}
}

namespace cc {
class ResourcePool;
class ScopedResource;

typedef int64 CopySequenceNumber;

class CC_EXPORT OneCopyRasterWorkerPool : public RasterWorkerPool,
                                          public Rasterizer,
                                          public RasterizerTaskClient {
 public:
  ~OneCopyRasterWorkerPool() override;

  static scoped_ptr<RasterWorkerPool> Create(
      base::SequencedTaskRunner* task_runner,
      TaskGraphRunner* task_graph_runner,
      ContextProvider* context_provider,
      ResourceProvider* resource_provider,
      ResourcePool* resource_pool);

  // Overridden from RasterWorkerPool:
  Rasterizer* AsRasterizer() override;

  // Overridden from Rasterizer:
  void SetClient(RasterizerClient* client) override;
  void Shutdown() override;
  void ScheduleTasks(RasterTaskQueue* queue) override;
  void CheckForCompletedTasks() override;

  // Overridden from RasterizerTaskClient:
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
  OneCopyRasterWorkerPool(base::SequencedTaskRunner* task_runner,
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

  void OnRasterFinished(TaskSet task_set);
  void AdvanceLastFlushedCopyTo(CopySequenceNumber sequence);
  void IssueCopyOperations(int64 count);
  void ScheduleCheckForCompletedCopyOperationsWithLockAcquired(
      bool wait_if_needed);
  void CheckForCompletedCopyOperations(bool wait_if_needed);
  scoped_refptr<base::debug::ConvertableToTraceFormat> StateAsValue() const;
  void StagingStateAsValueInto(base::debug::TracedValue* staging_state) const;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  TaskGraphRunner* task_graph_runner_;
  const NamespaceToken namespace_token_;
  RasterizerClient* client_;
  ContextProvider* context_provider_;
  ResourceProvider* resource_provider_;
  ResourcePool* resource_pool_;
  TaskSetCollection raster_pending_;
  scoped_refptr<RasterizerTask> raster_finished_tasks_[kNumberOfTaskSets];
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

  base::WeakPtrFactory<OneCopyRasterWorkerPool> weak_ptr_factory_;
  // "raster finished" tasks need their own factory as they need to be
  // canceled when ScheduleTasks() is called.
  base::WeakPtrFactory<OneCopyRasterWorkerPool>
      raster_finished_weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OneCopyRasterWorkerPool);
};

}  // namespace cc

#endif  // CC_RESOURCES_ONE_COPY_RASTER_WORKER_POOL_H_

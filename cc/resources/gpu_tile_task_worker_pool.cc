// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/gpu_tile_task_worker_pool.h"

#include <algorithm>

#include "base/debug/trace_event.h"
#include "cc/output/context_provider.h"
#include "cc/resources/raster_buffer.h"
#include "cc/resources/raster_source.h"
#include "cc/resources/resource.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/scoped_gpu_raster.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/skia/include/core/SkMultiPictureDraw.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace cc {
namespace {

class RasterBufferImpl : public RasterBuffer {
 public:
  RasterBufferImpl(ResourceProvider* resource_provider,
                   const Resource* resource,
                   SkMultiPictureDraw* multi_picture_draw,
                   bool use_distance_field_text)
      : lock_(resource_provider, resource->id()),
        resource_(resource),
        multi_picture_draw_(multi_picture_draw),
        use_distance_field_text_(use_distance_field_text) {}

  // Overridden from RasterBuffer:
  void Playback(const RasterSource* raster_source,
                const gfx::Rect& rect,
                float scale) override {
    // Turn on distance fields for layers that have ever animated.
    bool use_distance_field_text =
        use_distance_field_text_ ||
        raster_source->ShouldAttemptToUseDistanceFieldText();
    SkSurface* sk_surface = lock_.GetSkSurface(use_distance_field_text,
                                               raster_source->CanUseLCDText());

    if (!sk_surface)
      return;

    SkPictureRecorder recorder;
    gfx::Size size = resource_->size();
    const int flags = SkPictureRecorder::kComputeSaveLayerInfo_RecordFlag;
    skia::RefPtr<SkCanvas> canvas = skia::SharePtr(
        recorder.beginRecording(size.width(), size.height(), NULL, flags));

    canvas->save();
    raster_source->PlaybackToCanvas(canvas.get(), rect, scale);
    canvas->restore();

    // Add the canvas and recorded picture to |multi_picture_draw_|.
    skia::RefPtr<SkPicture> picture = skia::AdoptRef(recorder.endRecording());
    multi_picture_draw_->add(sk_surface->getCanvas(), picture.get());
  }

 private:
  ResourceProvider::ScopedWriteLockGr lock_;
  const Resource* resource_;
  SkMultiPictureDraw* multi_picture_draw_;
  bool use_distance_field_text_;

  DISALLOW_COPY_AND_ASSIGN(RasterBufferImpl);
};

}  // namespace

// static
scoped_ptr<TileTaskWorkerPool> GpuTileTaskWorkerPool::Create(
    base::SequencedTaskRunner* task_runner,
    ContextProvider* context_provider,
    ResourceProvider* resource_provider,
    bool use_distance_field_text) {
  return make_scoped_ptr<TileTaskWorkerPool>(
      new GpuTileTaskWorkerPool(task_runner, context_provider,
                                resource_provider, use_distance_field_text));
}

GpuTileTaskWorkerPool::GpuTileTaskWorkerPool(
    base::SequencedTaskRunner* task_runner,
    ContextProvider* context_provider,
    ResourceProvider* resource_provider,
    bool use_distance_field_text)
    : task_runner_(task_runner),
      task_graph_runner_(new TaskGraphRunner),
      namespace_token_(task_graph_runner_->GetNamespaceToken()),
      context_provider_(context_provider),
      resource_provider_(resource_provider),
      run_tasks_on_origin_thread_pending_(false),
      use_distance_field_text_(use_distance_field_text),
      task_set_finished_weak_ptr_factory_(this),
      weak_ptr_factory_(this) {
  DCHECK(context_provider_);
}

GpuTileTaskWorkerPool::~GpuTileTaskWorkerPool() {
  DCHECK_EQ(0u, completed_tasks_.size());
}

TileTaskRunner* GpuTileTaskWorkerPool::AsTileTaskRunner() {
  return this;
}

void GpuTileTaskWorkerPool::SetClient(TileTaskRunnerClient* client) {
  client_ = client;
}

void GpuTileTaskWorkerPool::Shutdown() {
  TRACE_EVENT0("cc", "GpuTileTaskWorkerPool::Shutdown");

  TaskGraph empty;
  task_graph_runner_->ScheduleTasks(namespace_token_, &empty);
  task_graph_runner_->WaitForTasksToFinishRunning(namespace_token_);
}

void GpuTileTaskWorkerPool::ScheduleTasks(TileTaskQueue* queue) {
  TRACE_EVENT0("cc", "GpuTileTaskWorkerPool::ScheduleTasks");

  // Mark all task sets as pending.
  tasks_pending_.set();

  unsigned priority = kTileTaskPriorityBase;

  graph_.Reset();

  // Cancel existing OnTaskSetFinished callbacks.
  task_set_finished_weak_ptr_factory_.InvalidateWeakPtrs();

  scoped_refptr<TileTask> new_task_set_finished_tasks[kNumberOfTaskSets];

  size_t task_count[kNumberOfTaskSets] = {0};

  for (TaskSet task_set = 0; task_set < kNumberOfTaskSets; ++task_set) {
    new_task_set_finished_tasks[task_set] = CreateTaskSetFinishedTask(
        task_runner_.get(),
        base::Bind(&GpuTileTaskWorkerPool::OnTaskSetFinished,
                   task_set_finished_weak_ptr_factory_.GetWeakPtr(), task_set));
  }

  for (TileTaskQueue::Item::Vector::const_iterator it = queue->items.begin();
       it != queue->items.end(); ++it) {
    const TileTaskQueue::Item& item = *it;
    RasterTask* task = item.task;
    DCHECK(!task->HasCompleted());

    for (TaskSet task_set = 0; task_set < kNumberOfTaskSets; ++task_set) {
      if (!item.task_sets[task_set])
        continue;

      ++task_count[task_set];

      graph_.edges.push_back(
          TaskGraph::Edge(task, new_task_set_finished_tasks[task_set].get()));
    }

    InsertNodesForRasterTask(&graph_, task, task->dependencies(), priority++);
  }

  for (TaskSet task_set = 0; task_set < kNumberOfTaskSets; ++task_set) {
    InsertNodeForTask(&graph_, new_task_set_finished_tasks[task_set].get(),
                      kTaskSetFinishedTaskPriority, task_count[task_set]);
  }

  ScheduleTasksOnOriginThread(this, &graph_);
  task_graph_runner_->ScheduleTasks(namespace_token_, &graph_);

  ScheduleRunTasksOnOriginThread();

  std::copy(new_task_set_finished_tasks,
            new_task_set_finished_tasks + kNumberOfTaskSets,
            task_set_finished_tasks_);
}

void GpuTileTaskWorkerPool::CheckForCompletedTasks() {
  TRACE_EVENT0("cc", "GpuTileTaskWorkerPool::CheckForCompletedTasks");

  task_graph_runner_->CollectCompletedTasks(namespace_token_,
                                            &completed_tasks_);
  for (Task::Vector::const_iterator it = completed_tasks_.begin();
       it != completed_tasks_.end(); ++it) {
    TileTask* task = static_cast<TileTask*>(it->get());

    task->WillComplete();
    task->CompleteOnOriginThread(this);
    task->DidComplete();

    task->RunReplyOnOriginThread();
  }
  completed_tasks_.clear();
}

scoped_ptr<RasterBuffer> GpuTileTaskWorkerPool::AcquireBufferForRaster(
    const Resource* resource) {
  return make_scoped_ptr<RasterBuffer>(
      new RasterBufferImpl(resource_provider_, resource, &multi_picture_draw_,
                           use_distance_field_text_));
}

void GpuTileTaskWorkerPool::ReleaseBufferForRaster(
    scoped_ptr<RasterBuffer> buffer) {
  // Nothing to do here. RasterBufferImpl destructor cleans up after itself.
}

void GpuTileTaskWorkerPool::OnTaskSetFinished(TaskSet task_set) {
  TRACE_EVENT1("cc", "GpuTileTaskWorkerPool::OnTaskSetFinished", "task_set",
               task_set);

  DCHECK(tasks_pending_[task_set]);
  tasks_pending_[task_set] = false;
  client_->DidFinishRunningTileTasks(task_set);
}

void GpuTileTaskWorkerPool::ScheduleRunTasksOnOriginThread() {
  if (run_tasks_on_origin_thread_pending_)
    return;

  task_runner_->PostTask(
      FROM_HERE, base::Bind(&GpuTileTaskWorkerPool::RunTasksOnOriginThread,
                            weak_ptr_factory_.GetWeakPtr()));
  run_tasks_on_origin_thread_pending_ = true;
}

void GpuTileTaskWorkerPool::RunTasksOnOriginThread() {
  TRACE_EVENT0("cc", "GpuTileTaskWorkerPool::RunTasksOnOriginThread");

  DCHECK(run_tasks_on_origin_thread_pending_);
  run_tasks_on_origin_thread_pending_ = false;

  ScopedGpuRaster gpu_raster(context_provider_);
  task_graph_runner_->RunUntilIdle();

  // Draw each all of the pictures that were collected.  This will also clear
  // the pictures and canvases added to |multi_picture_draw_|
  multi_picture_draw_.draw();
}

}  // namespace cc

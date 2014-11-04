// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/gpu_raster_worker_pool.h"

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
        raster_source->SuitableForDistanceFieldText();
    SkSurface* sk_surface = lock_.GetSkSurface(use_distance_field_text);

    if (!sk_surface)
      return;

    SkPictureRecorder recorder;
    gfx::Size size = resource_->size();
    skia::RefPtr<SkCanvas> canvas =
        skia::SharePtr(recorder.beginRecording(size.width(), size.height()));

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
scoped_ptr<RasterWorkerPool> GpuRasterWorkerPool::Create(
    base::SequencedTaskRunner* task_runner,
    ContextProvider* context_provider,
    ResourceProvider* resource_provider,
    bool use_distance_field_text) {
  return make_scoped_ptr<RasterWorkerPool>(
      new GpuRasterWorkerPool(task_runner,
                              context_provider,
                              resource_provider,
                              use_distance_field_text));
}

GpuRasterWorkerPool::GpuRasterWorkerPool(base::SequencedTaskRunner* task_runner,
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
      raster_finished_weak_ptr_factory_(this),
      weak_ptr_factory_(this) {
  DCHECK(context_provider_);
}

GpuRasterWorkerPool::~GpuRasterWorkerPool() {
  DCHECK_EQ(0u, completed_tasks_.size());
}

Rasterizer* GpuRasterWorkerPool::AsRasterizer() {
  return this;
}

void GpuRasterWorkerPool::SetClient(RasterizerClient* client) {
  client_ = client;
}

void GpuRasterWorkerPool::Shutdown() {
  TRACE_EVENT0("cc", "GpuRasterWorkerPool::Shutdown");

  TaskGraph empty;
  task_graph_runner_->ScheduleTasks(namespace_token_, &empty);
  task_graph_runner_->WaitForTasksToFinishRunning(namespace_token_);
}

void GpuRasterWorkerPool::ScheduleTasks(RasterTaskQueue* queue) {
  TRACE_EVENT0("cc", "GpuRasterWorkerPool::ScheduleTasks");

  // Mark all task sets as pending.
  raster_pending_.set();

  unsigned priority = kRasterTaskPriorityBase;

  graph_.Reset();

  // Cancel existing OnRasterFinished callbacks.
  raster_finished_weak_ptr_factory_.InvalidateWeakPtrs();

  scoped_refptr<RasterizerTask> new_raster_finished_tasks[kNumberOfTaskSets];

  size_t task_count[kNumberOfTaskSets] = {0};

  for (TaskSet task_set = 0; task_set < kNumberOfTaskSets; ++task_set) {
    new_raster_finished_tasks[task_set] = CreateRasterFinishedTask(
        task_runner_.get(),
        base::Bind(&GpuRasterWorkerPool::OnRasterFinished,
                   raster_finished_weak_ptr_factory_.GetWeakPtr(),
                   task_set));
  }

  for (RasterTaskQueue::Item::Vector::const_iterator it = queue->items.begin();
       it != queue->items.end();
       ++it) {
    const RasterTaskQueue::Item& item = *it;
    RasterTask* task = item.task;
    DCHECK(!task->HasCompleted());

    for (TaskSet task_set = 0; task_set < kNumberOfTaskSets; ++task_set) {
      if (!item.task_sets[task_set])
        continue;

      ++task_count[task_set];

      graph_.edges.push_back(
          TaskGraph::Edge(task, new_raster_finished_tasks[task_set].get()));
    }

    InsertNodesForRasterTask(&graph_, task, task->dependencies(), priority++);
  }

  for (TaskSet task_set = 0; task_set < kNumberOfTaskSets; ++task_set) {
    InsertNodeForTask(&graph_,
                      new_raster_finished_tasks[task_set].get(),
                      kRasterFinishedTaskPriority,
                      task_count[task_set]);
  }

  ScheduleTasksOnOriginThread(this, &graph_);
  task_graph_runner_->ScheduleTasks(namespace_token_, &graph_);

  ScheduleRunTasksOnOriginThread();

  std::copy(new_raster_finished_tasks,
            new_raster_finished_tasks + kNumberOfTaskSets,
            raster_finished_tasks_);
}

void GpuRasterWorkerPool::CheckForCompletedTasks() {
  TRACE_EVENT0("cc", "GpuRasterWorkerPool::CheckForCompletedTasks");

  task_graph_runner_->CollectCompletedTasks(namespace_token_,
                                            &completed_tasks_);
  for (Task::Vector::const_iterator it = completed_tasks_.begin();
       it != completed_tasks_.end();
       ++it) {
    RasterizerTask* task = static_cast<RasterizerTask*>(it->get());

    task->WillComplete();
    task->CompleteOnOriginThread(this);
    task->DidComplete();

    task->RunReplyOnOriginThread();
  }
  completed_tasks_.clear();
}

scoped_ptr<RasterBuffer> GpuRasterWorkerPool::AcquireBufferForRaster(
    const Resource* resource) {
  return make_scoped_ptr<RasterBuffer>(
      new RasterBufferImpl(resource_provider_,
                           resource,
                           &multi_picture_draw_,
                           use_distance_field_text_));
}

void GpuRasterWorkerPool::ReleaseBufferForRaster(
    scoped_ptr<RasterBuffer> buffer) {
  // Nothing to do here. RasterBufferImpl destructor cleans up after itself.
}

void GpuRasterWorkerPool::OnRasterFinished(TaskSet task_set) {
  TRACE_EVENT1(
      "cc", "GpuRasterWorkerPool::OnRasterFinished", "task_set", task_set);

  DCHECK(raster_pending_[task_set]);
  raster_pending_[task_set] = false;
  client_->DidFinishRunningTasks(task_set);
}

void GpuRasterWorkerPool::ScheduleRunTasksOnOriginThread() {
  if (run_tasks_on_origin_thread_pending_)
    return;

  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GpuRasterWorkerPool::RunTasksOnOriginThread,
                 weak_ptr_factory_.GetWeakPtr()));
  run_tasks_on_origin_thread_pending_ = true;
}

void GpuRasterWorkerPool::RunTasksOnOriginThread() {
  TRACE_EVENT0("cc", "GpuRasterWorkerPool::RunTasksOnOriginThread");

  DCHECK(run_tasks_on_origin_thread_pending_);
  run_tasks_on_origin_thread_pending_ = false;

  ScopedGpuRaster gpu_raster(context_provider_);
  task_graph_runner_->RunUntilIdle();

  // Draw each all of the pictures that were collected.  This will also clear
  // the pictures and canvases added to |multi_picture_draw_|
  multi_picture_draw_.draw();
}

}  // namespace cc

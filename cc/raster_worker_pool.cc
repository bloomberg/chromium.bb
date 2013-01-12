// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster_worker_pool.h"

#include <algorithm>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "cc/picture_pile_impl.h"
#include "third_party/skia/include/core/SkDevice.h"

namespace cc {

namespace {

void RunRasterTask(PicturePileImpl* picture_pile,
                   uint8* buffer,
                   const gfx::Rect& rect,
                   float contents_scale,
                   RenderingStats* stats) {
  TRACE_EVENT0("cc", "RunRasterTask");
  DCHECK(picture_pile);
  DCHECK(buffer);
  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, rect.width(), rect.height());
  bitmap.setPixels(buffer);
  SkDevice device(bitmap);
  SkCanvas canvas(&device);
  picture_pile->Raster(&canvas, rect, contents_scale, stats);
}

void RunImageDecodeTask(skia::LazyPixelRef* pixel_ref, RenderingStats* stats) {
  TRACE_EVENT0("cc", "RunImageDecodeTask");
  base::TimeTicks decode_begin_time = base::TimeTicks::Now();
  pixel_ref->Decode();
  stats->totalDeferredImageDecodeCount++;
  stats->totalDeferredImageDecodeTimeInSeconds +=
      (base::TimeTicks::Now() - decode_begin_time).InSecondsF();
}

const char* kRasterThreadNamePrefix = "CompositorRaster";

// Allow two pending raster tasks per thread. This keeps resource usage
// low while making sure raster threads aren't unnecessarily idle.
const int kNumPendingRasterTasksPerThread = 2;

}  // namespace

RasterWorkerPool::Thread::Task::Task(Thread* thread) : thread_(thread) {
  thread_->num_pending_tasks_++;
}

RasterWorkerPool::Thread::Task::~Task() {
  thread_->rendering_stats_.totalRasterizeTimeInSeconds +=
      rendering_stats_.totalRasterizeTimeInSeconds;
  thread_->rendering_stats_.totalPixelsRasterized +=
      rendering_stats_.totalPixelsRasterized;
  thread_->rendering_stats_.totalDeferredImageDecodeTimeInSeconds +=
      rendering_stats_.totalDeferredImageDecodeTimeInSeconds;
  thread_->rendering_stats_.totalDeferredImageDecodeCount +=
      rendering_stats_.totalDeferredImageDecodeCount;

  thread_->num_pending_tasks_--;
}

RasterWorkerPool::Thread::Thread(const std::string name)
    : base::Thread(name.c_str()),
      num_pending_tasks_(0) {
  Start();
}

RasterWorkerPool::Thread::~Thread() {
  Stop();
}

RasterWorkerPool::RasterWorkerPool(size_t num_raster_threads) {
  const std::string thread_name_prefix = kRasterThreadNamePrefix;
  while (raster_threads_.size() < num_raster_threads) {
    int thread_number = raster_threads_.size() + 1;
    raster_threads_.push_back(
        new Thread(thread_name_prefix +
                   StringPrintf("Worker%d", thread_number).c_str()));
  }
}

RasterWorkerPool::~RasterWorkerPool() {
  STLDeleteElements(&raster_threads_);
}

bool RasterWorkerPool::IsBusy() {
  Thread* thread = raster_threads_.front();
  return thread->num_pending_tasks() >= kNumPendingRasterTasksPerThread;
}

void RasterWorkerPool::PostRasterTaskAndReply(PicturePileImpl* picture_pile,
                                              uint8* buffer,
                                              const gfx::Rect& rect,
                                              float contents_scale,
                                              const base::Closure& reply) {
  Thread::Task* task = CreateTask();

  scoped_refptr<PicturePileImpl> picture_pile_clone =
      picture_pile->GetCloneForDrawingOnThread(task->thread_);

  task->thread_->message_loop_proxy()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&RunRasterTask,
                 base::Unretained(picture_pile_clone.get()),
                 buffer,
                 rect,
                 contents_scale,
                 &task->rendering_stats_),
      base::Bind(&RasterWorkerPool::OnRasterTaskCompleted,
                 base::Unretained(this),
                 base::Unretained(task),
                 picture_pile_clone,
                 reply));
}

void RasterWorkerPool::PostImageDecodeTaskAndReply(
    skia::LazyPixelRef* pixel_ref,
    const base::Closure& reply) {
  Thread::Task* task = CreateTask();

  task->thread_->message_loop_proxy()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&RunImageDecodeTask, pixel_ref, &task->rendering_stats_),
      base::Bind(&RasterWorkerPool::OnTaskCompleted,
                 base::Unretained(this),
                 base::Unretained(task),
                 reply));
}

void RasterWorkerPool::GetRenderingStats(RenderingStats* stats) {
  stats->totalRasterizeTimeInSeconds = 0;
  stats->totalPixelsRasterized = 0;
  stats->totalDeferredImageDecodeCount = 0;
  stats->totalDeferredImageDecodeTimeInSeconds = 0;
  for (ThreadVector::iterator it = raster_threads_.begin();
       it != raster_threads_.end(); ++it) {
    Thread* thread = *it;
    stats->totalRasterizeTimeInSeconds +=
        thread->rendering_stats().totalRasterizeTimeInSeconds;
    stats->totalPixelsRasterized +=
        thread->rendering_stats().totalPixelsRasterized;
    stats->totalDeferredImageDecodeCount +=
        thread->rendering_stats().totalDeferredImageDecodeCount;
    stats->totalDeferredImageDecodeTimeInSeconds +=
        thread->rendering_stats().totalDeferredImageDecodeTimeInSeconds;
  }
}

RasterWorkerPool::Thread::Task* RasterWorkerPool::CreateTask() {
  Thread* thread = raster_threads_.front();
  DCHECK(thread->num_pending_tasks() < kNumPendingRasterTasksPerThread);

  scoped_ptr<Thread::Task> task(new Thread::Task(thread));
  std::sort(raster_threads_.begin(), raster_threads_.end(),
            PendingTaskComparator());
  return task.release();
}

void RasterWorkerPool::DestroyTask(Thread::Task* task) {
  delete task;
  std::sort(raster_threads_.begin(), raster_threads_.end(),
            PendingTaskComparator());
}

void RasterWorkerPool::OnTaskCompleted(
    Thread::Task* task, const base::Closure& reply) {
  DestroyTask(task);
  reply.Run();
}

void RasterWorkerPool::OnRasterTaskCompleted(
    Thread::Task* task,
    scoped_refptr<PicturePileImpl> picture_pile,
    const base::Closure& reply) {
  OnTaskCompleted(task, reply);
}

}  // namespace cc

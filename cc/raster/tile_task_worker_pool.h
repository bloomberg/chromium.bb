// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_TILE_TASK_WORKER_POOL_H_
#define CC_RASTER_TILE_TASK_WORKER_POOL_H_

#include <stddef.h>

#include "cc/playback/raster_source.h"
#include "cc/raster/raster_buffer.h"
#include "cc/raster/task_graph_runner.h"
#include "cc/raster/tile_task.h"
#include "cc/resources/resource_format.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class SequencedTaskRunner;
}

namespace cc {
class RenderingStatsInstrumentation;

// This class provides the wrapper over TaskGraphRunner for scheduling and
// collecting tasks. The client can call CheckForCompletedTasks() at any time to
// process all completed tasks at the moment that have finished running or
// cancelled.
class CC_EXPORT TileTaskWorkerPool {
 public:
  TileTaskWorkerPool();
  virtual ~TileTaskWorkerPool();

  // Utility function that can be used to call ::ScheduleOnOriginThread() for
  // each task in |graph|.
  static void ScheduleTasksOnOriginThread(RasterBufferProvider* provider,
                                          TaskGraph* graph);

  // Utility function that will create a temporary bitmap and copy pixels to
  // |memory| when necessary. The |canvas_bitmap_rect| is the rect of the bitmap
  // being played back in the pixel space of the source, ie a rect in the source
  // that will cover the resulting |memory|. The |canvas_playback_rect| can be a
  // smaller contained rect inside the |canvas_bitmap_rect| if the |memory| is
  // already partially complete, and only the subrect needs to be played back.
  static void PlaybackToMemory(
      void* memory,
      ResourceFormat format,
      const gfx::Size& size,
      size_t stride,
      const RasterSource* raster_source,
      const gfx::Rect& canvas_bitmap_rect,
      const gfx::Rect& canvas_playback_rect,
      float scale,
      const RasterSource::PlaybackSettings& playback_settings);

  // Tells the worker pool to shutdown after canceling all previously scheduled
  // tasks. Reply callbacks are still guaranteed to run when
  // CheckForCompletedTasks() is called.
  virtual void Shutdown() = 0;

  // Schedule running of tile tasks in |graph| and all dependencies.
  // Previously scheduled tasks that are not in |graph| will be canceled unless
  // already running. Once scheduled, reply callbacks are guaranteed to run for
  // all tasks even if they later get canceled by another call to
  // ScheduleTasks().
  virtual void ScheduleTasks(TaskGraph* graph) = 0;

  // Check for completed tasks and dispatch reply callbacks.
  virtual void CheckForCompletedTasks() = 0;

  // Returns the format to use for the tiles.
  virtual ResourceFormat GetResourceFormat(bool must_support_alpha) const = 0;

  // Determine if the resource requires swizzling.
  virtual bool GetResourceRequiresSwizzle(bool must_support_alpha) const = 0;

  // Downcasting routine for RasterBufferProvider interface.
  virtual RasterBufferProvider* AsRasterBufferProvider() = 0;

 protected:
  // Check if resource format matches output format.
  static bool ResourceFormatRequiresSwizzle(ResourceFormat format);
};

}  // namespace cc

#endif  // CC_RASTER_TILE_TASK_WORKER_POOL_H_

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_TILE_TASK_WORKER_POOL_H_
#define CC_RASTER_TILE_TASK_WORKER_POOL_H_

#include <stddef.h>

#include "cc/raster/tile_task_runner.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class SequencedTaskRunner;
}

namespace cc {
class DisplayListRasterSource;
class RenderingStatsInstrumentation;

class CC_EXPORT TileTaskWorkerPool {
 public:
  TileTaskWorkerPool();
  virtual ~TileTaskWorkerPool();

  // Utility function that can be used to call ::ScheduleOnOriginThread() for
  // each task in |graph|.
  static void ScheduleTasksOnOriginThread(TileTaskClient* client,
                                          TaskGraph* graph);

  // Utility function that will create a temporary bitmap and copy pixels to
  // |memory| when necessary. The |canvas_bitmap_rect| is the rect of the bitmap
  // being played back in the pixel space of the source, ie a rect in the source
  // that will cover the resulting |memory|. The |canvas_playback_rect| can be a
  // smaller contained rect inside the |canvas_bitmap_rect| if the |memory| is
  // already partially complete, and only the subrect needs to be played back.
  static void PlaybackToMemory(void* memory,
                               ResourceFormat format,
                               const gfx::Size& size,
                               size_t stride,
                               const DisplayListRasterSource* raster_source,
                               const gfx::Rect& canvas_bitmap_rect,
                               const gfx::Rect& canvas_playback_rect,
                               float scale,
                               bool include_images);

  // Type-checking downcast routine.
  virtual TileTaskRunner* AsTileTaskRunner() = 0;
};

}  // namespace cc

#endif  // CC_RASTER_TILE_TASK_WORKER_POOL_H_

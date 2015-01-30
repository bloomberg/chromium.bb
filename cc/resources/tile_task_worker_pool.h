// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_TILE_TASK_WORKER_POOL_H_
#define CC_RESOURCES_TILE_TASK_WORKER_POOL_H_

#include "base/threading/platform_thread.h"
#include "cc/resources/tile_task_runner.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class SequencedTaskRunner;
}

namespace cc {
class RasterSource;
class RenderingStatsInstrumentation;

class CC_EXPORT TileTaskWorkerPool {
 public:
  static unsigned kBenchmarkTaskPriority;
  static unsigned kTaskSetFinishedTaskPriorityBase;
  static unsigned kTileTaskPriorityBase;

  TileTaskWorkerPool();
  virtual ~TileTaskWorkerPool();

  // Set the number of threads to use for the global TaskGraphRunner instance.
  // This can only be called once and must be called prior to
  // GetNumWorkerThreads().
  static void SetNumWorkerThreads(int num_threads);

  // Returns the number of threads used for the global TaskGraphRunner instance.
  static int GetNumWorkerThreads();

  // Set the priority of worker threads.
  static void SetWorkerThreadPriority(base::ThreadPriority priority);

  // Returns a pointer to the global TaskGraphRunner instance.
  static TaskGraphRunner* GetTaskGraphRunner();

  // Utility function that can be used to create a "Task set finished" task that
  // posts |callback| to |task_runner| when run.
  static scoped_refptr<TileTask> CreateTaskSetFinishedTask(
      base::SequencedTaskRunner* task_runner,
      const base::Closure& callback);

  // Utility function that can be used to call ::ScheduleOnOriginThread() for
  // each task in |graph|.
  static void ScheduleTasksOnOriginThread(TileTaskClient* client,
                                          TaskGraph* graph);

  // Utility function that can be used to build a task graph. Inserts a node
  // that represents |task| in |graph|. See TaskGraph definition for valid
  // |priority| values.
  static void InsertNodeForTask(TaskGraph* graph,
                                TileTask* task,
                                unsigned priority,
                                size_t dependencies);

  // Utility function that can be used to build a task graph. Inserts nodes that
  // represent |task| and all its image decode dependencies in |graph|.
  static void InsertNodesForRasterTask(
      TaskGraph* graph,
      RasterTask* task,
      const ImageDecodeTask::Vector& decode_tasks,
      unsigned priority);

  // Utility function that will create a temporary bitmap and copy pixels to
  // |memory| when necessary.
  static void PlaybackToMemory(void* memory,
                               ResourceFormat format,
                               const gfx::Size& size,
                               int stride,
                               const RasterSource* raster_source,
                               const gfx::Rect& rect,
                               float scale);

  // Type-checking downcast routine.
  virtual TileTaskRunner* AsTileTaskRunner() = 0;
};

}  // namespace cc

#endif  // CC_RESOURCES_TILE_TASK_WORKER_POOL_H_

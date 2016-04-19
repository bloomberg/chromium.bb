// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_TILE_TASK_RUNNER_H_
#define CC_RASTER_TILE_TASK_RUNNER_H_

#include <stdint.h>

#include <vector>

#include "base/callback.h"
#include "cc/raster/raster_buffer.h"
#include "cc/raster/task.h"
#include "cc/resources/resource_format.h"

namespace cc {

class CC_EXPORT TileTask : public Task {
 public:
  typedef std::vector<scoped_refptr<TileTask>> Vector;

  const TileTask::Vector& dependencies() const { return dependencies_; }

  // Indicates whether this TileTask can be run at the same time as other tasks
  // in the task graph. If false, this task will be scheduled with
  // TASK_CATEGORY_NONCONCURRENT_FOREGROUND. The base implementation always
  // returns true.
  bool SupportsConcurrentExecution() const {
    return supports_concurrent_execution_;
  }

  virtual void ScheduleOnOriginThread(RasterBufferProvider* provider) = 0;
  virtual void CompleteOnOriginThread(RasterBufferProvider* provider) = 0;

  void WillSchedule();
  void DidSchedule();
  bool HasBeenScheduled() const;

  void WillComplete();
  void DidComplete();
  bool HasCompleted() const;

 protected:
  explicit TileTask(bool supports_concurrent_execution);
  TileTask(bool supports_concurrent_execution, TileTask::Vector* dependencies);
  ~TileTask() override;

  const bool supports_concurrent_execution_;
  TileTask::Vector dependencies_;
  bool did_schedule_;
  bool did_complete_;
};

// This interface can be used to schedule and run tile tasks.
// The client can call CheckForCompletedTasks() at any time to dispatch
// pending completion callbacks for all tasks that have finished running.
class CC_EXPORT TileTaskRunner {
 public:
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

  virtual ~TileTaskRunner() {}
};

}  // namespace cc

#endif  // CC_RASTER_TILE_TASK_RUNNER_H_

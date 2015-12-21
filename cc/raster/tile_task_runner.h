// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_TILE_TASK_RUNNER_H_
#define CC_RASTER_TILE_TASK_RUNNER_H_

#include <stdint.h>

#include <vector>

#include "base/callback.h"
#include "cc/raster/task_graph_runner.h"
#include "cc/resources/resource_format.h"

namespace cc {
class ImageDecodeTask;
class RasterTask;
class Resource;
class RasterBuffer;

class CC_EXPORT TileTaskClient {
 public:
  virtual scoped_ptr<RasterBuffer> AcquireBufferForRaster(
      const Resource* resource,
      uint64_t resource_content_id,
      uint64_t previous_content_id) = 0;
  virtual void ReleaseBufferForRaster(scoped_ptr<RasterBuffer> buffer) = 0;

 protected:
  virtual ~TileTaskClient() {}
};

class CC_EXPORT TileTask : public Task {
 public:
  typedef std::vector<scoped_refptr<TileTask>> Vector;

  virtual void ScheduleOnOriginThread(TileTaskClient* client) = 0;
  virtual void CompleteOnOriginThread(TileTaskClient* client) = 0;

  void WillSchedule();
  void DidSchedule();
  bool HasBeenScheduled() const;

  void WillComplete();
  void DidComplete();
  bool HasCompleted() const;

 protected:
  TileTask();
  ~TileTask() override;

  bool did_schedule_;
  bool did_complete_;
};

class CC_EXPORT ImageDecodeTask : public TileTask {
 public:
  typedef std::vector<scoped_refptr<ImageDecodeTask>> Vector;

 protected:
  ImageDecodeTask();
  ~ImageDecodeTask() override;
};

class CC_EXPORT RasterTask : public TileTask {
 public:
  typedef std::vector<scoped_refptr<RasterTask>> Vector;

  const ImageDecodeTask::Vector& dependencies() const { return dependencies_; }

 protected:
  explicit RasterTask(ImageDecodeTask::Vector* dependencies);
  ~RasterTask() override;

 private:
  ImageDecodeTask::Vector dependencies_;
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

 protected:
  virtual ~TileTaskRunner() {}
};

}  // namespace cc

#endif  // CC_RASTER_TILE_TASK_RUNNER_H_

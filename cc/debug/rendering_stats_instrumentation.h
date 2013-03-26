// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DEBUG_RENDERING_STATS_INSTRUMENTATION_H_
#define CC_DEBUG_RENDERING_STATS_INSTRUMENTATION_H_

#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "cc/debug/rendering_stats.h"

namespace cc {

// RenderingStatsInstrumentation is shared among threads and manages conditional
// recording of rendering stats into a private RenderingStats instance.
class CC_EXPORT RenderingStatsInstrumentation {
 public:
  static scoped_ptr<RenderingStatsInstrumentation> Create();
  virtual ~RenderingStatsInstrumentation();

  RenderingStats GetRenderingStats();

  // Read and write access to the record_rendering_stats_ flag is not locked to
  // improve performance. The flag is commonly turned off and hardly changes
  // it's value during runtime.
  bool record_rendering_stats() const { return record_rendering_stats_; }
  void set_record_rendering_stats(bool record_rendering_stats) {
    if (record_rendering_stats_ != record_rendering_stats)
      record_rendering_stats_ = record_rendering_stats;
  }

  base::TimeTicks StartRecording() const;
  base::TimeDelta EndRecording(base::TimeTicks start_time) const;

  // TODO(egraether): Remove after switching Layer::update() to use this class.
  // Used in LayerTreeHost::paintLayerContents().
  void AddStats(const RenderingStats& other);

  void IncrementAnimationFrameCount();
  void SetScreenFrameCount(int64 count);
  void SetDroppedFrameCount(int64 count);

  void AddCommit(base::TimeDelta duration);
  void AddPaint(base::TimeDelta duration, int64 pixels);
  void AddRaster(base::TimeDelta duration,
                 int64 pixels,
                 bool is_in_pending_tree_now_bin);

  void IncrementImplThreadScrolls();
  void IncrementMainThreadScrolls();

  void AddLayersDrawn(int64 amount);
  void AddMissingTiles(int64 amount);

  void AddDeferredImageDecode(base::TimeDelta duration);
  void AddImageGathering(base::TimeDelta duration);

  void IncrementDeferredImageCacheHitCount();

 protected:
  RenderingStatsInstrumentation();

 private:
  RenderingStats rendering_stats_;
  bool record_rendering_stats_;

  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(RenderingStatsInstrumentation);
};

}  // namespace cc

#endif  // CC_DEBUG_RENDERING_STATS_INSTRUMENTATION_H_

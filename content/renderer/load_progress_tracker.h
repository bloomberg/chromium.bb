// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_LOAD_PROGRESS_TRACKER_H_
#define CONTENT_RENDERER_LOAD_PROGRESS_TRACKER_H_

#include "base/containers/hash_tables.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"

namespace content {
class RenderViewImpl;

class LoadProgressTracker {
 public:
  explicit LoadProgressTracker(RenderViewImpl* render_view);
  ~LoadProgressTracker();

  void DidStartLoading(int frame_routing_id);
  void DidStopLoading(int frame_routing_id);

  void DidChangeLoadProgress(int frame_routing_id, double progress);

 private:
  void ResetStates();

  void SendChangeLoadProgress();

  RenderViewImpl* render_view_;

  // ProgressMap maps RenderFrame routing ids to a double representing that
  // frame's completion (from 0 to 1).
  typedef base::hash_map<int, double> ProgressMap;
  ProgressMap progresses_;
  double total_progress_;

  base::TimeTicks last_time_progress_sent_;

  base::WeakPtrFactory<LoadProgressTracker> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(LoadProgressTracker);
};

}  // namespace content

#endif  // CONTENT_RENDERER_LOAD_PROGRESS_TRACKER_H_

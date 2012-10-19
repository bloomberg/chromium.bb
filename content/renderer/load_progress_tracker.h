// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_LOAD_PROGRESS_TRACKER_H_
#define CONTENT_RENDERER_LOAD_PROGRESS_TRACKER_H_

#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"

namespace WebKit {
class WebFrame;
}

namespace content {
class RenderViewImpl;

class LoadProgressTracker {
 public:
  explicit LoadProgressTracker(RenderViewImpl* render_view);
  ~LoadProgressTracker();

  void DidStopLoading();

  void DidChangeLoadProgress(WebKit::WebFrame* frame, double progress);

 private:
  void ResetStates();

  void SendChangeLoadProgress();

  RenderViewImpl* render_view_;

  WebKit::WebFrame* tracked_frame_;

  double progress_;

  base::TimeTicks last_time_progress_sent_;

  base::WeakPtrFactory<LoadProgressTracker> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(LoadProgressTracker);
};

}  // namespace content

#endif  // CONTENT_RENDERER_LOAD_PROGRESS_TRACKER_H_

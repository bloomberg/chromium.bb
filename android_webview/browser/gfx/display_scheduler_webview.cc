// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/display_scheduler_webview.h"
#include "android_webview/browser/gfx/root_frame_sink.h"

namespace android_webview {
DisplaySchedulerWebView::DisplaySchedulerWebView(RootFrameSink* root_frame_sink)
    : root_frame_sink_(root_frame_sink) {}
DisplaySchedulerWebView::~DisplaySchedulerWebView() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void DisplaySchedulerWebView::ForceImmediateSwapIfPossible() {
  // We can't swap immediately
  NOTREACHED();
}
void DisplaySchedulerWebView::SetNeedsOneBeginFrame(bool needs_draw) {
  // Used with De-Jelly and headless begin frames
  NOTREACHED();
}
void DisplaySchedulerWebView::DidSwapBuffers() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  bool needs_draw = false;
  for (auto it = damaged_frames_.begin(); it != damaged_frames_.end();) {
    DCHECK_GT(it->second, 0);
    if (!--it->second) {
      it = damaged_frames_.erase(it);
    } else {
      if (!needs_draw) {
        TRACE_EVENT_INSTANT2(
            "android_webview",
            "DisplaySchedulerWebView::DidSwapBuffers first needs_draw",
            TRACE_EVENT_SCOPE_THREAD, "frame_sink_id", it->first.ToString(),
            "damage_count", it->second);
      }
      needs_draw = true;
      ++it;
    }
  }

  root_frame_sink_->SetNeedsDraw(needs_draw);
}
void DisplaySchedulerWebView::OutputSurfaceLost() {
  // WebView can't handle surface lost so this isn't called.
  NOTREACHED();
}
void DisplaySchedulerWebView::OnDisplayDamaged(viz::SurfaceId surface_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // We don't need to track damage of root frame sink as we submit frame to it
  // at DrawAndSwap and Root Renderer sink because Android View.Invalidation is
  // handled by SynchronousCompositorHost.
  if (surface_id.frame_sink_id() != root_frame_sink_->root_frame_sink_id() &&
      !root_frame_sink_->IsChildSurface(surface_id.frame_sink_id())) {
    int count = damaged_frames_[surface_id.frame_sink_id()] + 1;

    TRACE_EVENT_INSTANT2(
        "android_webview", "DisplaySchedulerWebView::OnDisplayDamaged",
        TRACE_EVENT_SCOPE_THREAD, "frame_sink_id",
        surface_id.frame_sink_id().ToString(), "damage_count", count);

    // Clamp value to max two frames. Two is enough to keep invalidation
    // working, but will prevent number going too high in case if kModeDraw
    // doesn't happen for some time.
    damaged_frames_[surface_id.frame_sink_id()] = std::min(2, count);

    root_frame_sink_->SetNeedsDraw(true);
  }
}
}  // namespace android_webview

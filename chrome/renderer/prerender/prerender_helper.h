// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PRERENDER_PRERENDER_HELPER_H_
#define CHROME_RENDERER_PRERENDER_PRERENDER_HELPER_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/time.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/public/renderer/render_view_observer_tracker.h"

namespace prerender {

// Helper class to track whether its RenderView is currently being prerendered.
// Also records prerendering-related histograms information and cancels
// prerendering when necessary, based on observed events.  Created when
// prerendering starts and deleted as soon as just after the prerendering
// histograms have been recorded for a displayed prerendered page.  For
// non-displayed pages, deleted on destruction of the RenderView.
class PrerenderHelper
    : public content::RenderViewObserver,
      public content::RenderViewObserverTracker<PrerenderHelper> {
 public:
  explicit PrerenderHelper(RenderView* render_view);
  virtual ~PrerenderHelper();

  // Returns true if |render_view| is currently prerendering.
  static bool IsPrerendering(const RenderView* render_view);

  // Records prerender histograms.  These are recorded even for pages that are
  // not prerendered, for comparison to pages that are.
  static void RecordHistograms(
      RenderView* render_view,
      const base::Time& finish_all_loads,
      const base::TimeDelta& begin_to_finish_all_loads);

 private:
  // RenderViewObserver implementation
  virtual void DidStartProvisionalLoad(WebKit::WebFrame* frame) OVERRIDE;
  virtual void WillCreateMediaPlayer(
      WebKit::WebFrame* frame,
      WebKit::WebMediaPlayerClient* client) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnSetIsPrerendering(bool is_prerendering);

  // Returns true if the page is no longer being prerendered, but no histograms
  // for the prerender have been recorded.
  bool HasUnrecordedData() const;

  // Updates the visibility state of the RenderView.  Must be called whenever
  // prerendering starts or finishes.
  void UpdateVisibilityState();

  // Tracks whether or not observed RenderView is currently prerendering.
  bool is_prerendering_;

  // Time when the prerender started.
  base::Time prerender_start_time_;
  // Time when the prerendered page was displayed.
  base::Time prerender_display_time_;

  // Set to true when a prerendered page is displayed to prevent deletion from
  // when a prerendered page is displayed until after the histograms for the
  // page load have been recorded.
  bool has_unrecorded_data_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderHelper);
};

}  // namespace prerender

#endif  // CHROME_RENDERER_PRERENDER_PRERENDER_HELPER_H_

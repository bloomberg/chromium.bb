// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PRERENDER_PRERENDER_HELPER_H_
#define CHROME_RENDERER_PRERENDER_PRERENDER_HELPER_H_

#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"

namespace prerender {

// Helper class to track whether its RenderFrame is currently being prerendered.
// Created when prerendering starts and deleted as soon as it stops.
class PrerenderHelper
    : public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<PrerenderHelper> {
 public:
  explicit PrerenderHelper(content::RenderFrame* render_frame);
  virtual ~PrerenderHelper();

  // Returns true if |render_view| is currently prerendering.
  static bool IsPrerendering(const content::RenderFrame* render_frame);

 private:
  // RenderViewObserver implementation
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnSetIsPrerendering(bool is_prerendering);

  DISALLOW_COPY_AND_ASSIGN(PrerenderHelper);
};

}  // namespace prerender

#endif  // CHROME_RENDERER_PRERENDER_PRERENDER_HELPER_H_

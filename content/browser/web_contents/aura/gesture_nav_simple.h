// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_AURA_GESTURE_NAV_SIMPLE_H_
#define CONTENT_BROWSER_WEB_CONTENTS_AURA_GESTURE_NAV_SIMPLE_H_

#include <memory>

#include "base/macros.h"
#include "content/browser/renderer_host/overscroll_controller_delegate.h"

namespace content {

class WebContentsImpl;

// A simple delegate for the overscroll controller that paints an arrow on top
// of the web-contents as a hint for pending navigations from overscroll.
class GestureNavSimple : public OverscrollControllerDelegate {
 public:
  explicit GestureNavSimple(WebContentsImpl* web_contents);
  ~GestureNavSimple() override;

 private:
  class Affordance;

  void AbortGestureAnimation();
  void CompleteGestureAnimation();

  // OverscrollControllerDelegate:
  gfx::Rect GetVisibleBounds() const override;
  bool OnOverscrollUpdate(float delta_x, float delta_y) override;
  void OnOverscrollComplete(OverscrollMode overscroll_mode) override;
  void OnOverscrollModeChange(OverscrollMode old_mode,
                              OverscrollMode new_mode,
                              OverscrollSource source) override;

  WebContentsImpl* web_contents_;
  std::unique_ptr<Affordance> affordance_;
  float completion_threshold_;

  DISALLOW_COPY_AND_ASSIGN(GestureNavSimple);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_AURA_GESTURE_NAV_SIMPLE_H_

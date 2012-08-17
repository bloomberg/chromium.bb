// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_WEBVIEW_ANIMATING_OVERLAY_H_
#define CHROME_RENDERER_WEBVIEW_ANIMATING_OVERLAY_H_

#include "base/compiler_specific.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPageOverlay.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/animation/linear_animation.h"

namespace content {
class RenderView;
}

// This class draws a gradient on a PageOverlay of a WebView. The gradient
// fades in when shown and fades out when hidden.
class WebViewAnimatingOverlay : public WebKit::WebPageOverlay,
                                public ui::AnimationDelegate {
 public:
  enum State {
    ANIMATING_IN,
    VISIBLE,
    ANIMATING_OUT,
    HIDDEN
  };

  explicit WebViewAnimatingOverlay(content::RenderView* render_view);
  virtual ~WebViewAnimatingOverlay();
  void Show();
  void Hide();

  State state() { return state_; }

  // WebKit::WebPageOverlay implementation:
  virtual void paintPageOverlay(WebKit::WebCanvas* canvas) OVERRIDE;

  // ui::AnimationDelegate implementation:
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;

 private:
  int GetCurrentAlpha();

  content::RenderView* render_view_;
  State state_;
  ui::LinearAnimation animation_;

  DISALLOW_COPY_AND_ASSIGN(WebViewAnimatingOverlay);
};

#endif  // CHROME_RENDERER_WEBVIEW_ANIMATING_OVERLAY_H_

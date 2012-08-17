// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/webview_animating_overlay.h"

#include "base/logging.h"
#include "content/public/renderer/render_view.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/gfx/size.h"
#include "ui/gfx/skia_util.h"

const int kAnimationDurationMiliseconds = 500;
const int kAnimationFrameRate = 60;
const int kTargetAlpha = 191;

WebViewAnimatingOverlay::WebViewAnimatingOverlay(
    content::RenderView* render_view)
    : render_view_(render_view),
      state_(HIDDEN),
      animation_(kAnimationDurationMiliseconds, kAnimationFrameRate, this) {
}

WebViewAnimatingOverlay::~WebViewAnimatingOverlay() {
  if (render_view_->GetWebView())
    render_view_->GetWebView()->removePageOverlay(this);
}

void WebViewAnimatingOverlay::Show() {
  if (state_ == ANIMATING_IN || state_ == VISIBLE)
    return;

  if (state_ == ANIMATING_OUT) {
    animation_.SetCurrentValue(1.0 - animation_.GetCurrentValue());
  } else {
    DCHECK_EQ(HIDDEN, state_);
    if (render_view_->GetWebView())
      render_view_->GetWebView()->addPageOverlay(this, 0);
  }

  state_ = ANIMATING_IN;
  animation_.Start();
}

void WebViewAnimatingOverlay::Hide() {
  if (state_ == ANIMATING_OUT || state_ == HIDDEN)
    return;

  if (state_ == ANIMATING_IN)
    animation_.SetCurrentValue(1.0 - animation_.GetCurrentValue());

  state_ = ANIMATING_OUT;
  animation_.Start();
}

void WebViewAnimatingOverlay::paintPageOverlay(WebKit::WebCanvas* canvas) {
  SkRect rect = gfx::RectToSkRect(gfx::Rect(render_view_->GetSize()));

  // The center of the radial gradient should be near the top middle.
  SkPoint center_point;
  center_point.iset(rect.width() * 0.5, rect.height() * 0.05);
  int radius = static_cast<int>(rect.width());

  // Animate in or out using the alpha.
  int alpha = GetCurrentAlpha();
  SkColor colors[] = {
    SkColorSetARGB(alpha, 255, 255, 255),
    SkColorSetARGB(alpha, 127, 127, 127)
  };

  SkAutoTUnref<SkShader> shader(SkGradientShader::CreateRadial(center_point,
      SkIntToScalar(radius), colors, NULL, arraysize(colors),
      SkShader::kClamp_TileMode));
  SkPaint paint;
  paint.setShader(shader);
  paint.setDither(true);

  canvas->drawRect(rect, paint);
}

void WebViewAnimatingOverlay::AnimationEnded(const ui::Animation* animation) {
  if (state_ == ANIMATING_IN) {
    state_ = VISIBLE;
  } else {
    DCHECK_EQ(ANIMATING_OUT, state_);
    state_ = HIDDEN;
    if (render_view_->GetWebView())
      render_view_->GetWebView()->removePageOverlay(this);
  }
}

void WebViewAnimatingOverlay::AnimationProgressed(
    const ui::Animation* animation) {
  render_view_->Repaint(render_view_->GetSize());
}

int WebViewAnimatingOverlay::GetCurrentAlpha() {
  switch (state_) {
    case ANIMATING_IN:
      return animation_.CurrentValueBetween(0, kTargetAlpha);
    case ANIMATING_OUT:
      return animation_.CurrentValueBetween(kTargetAlpha, 0);
    case VISIBLE:
    case HIDDEN:
      return kTargetAlpha;
  }
  NOTREACHED();
  return 1.0;
}

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/webview_color_overlay.h"

#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/public/web/WebGraphicsContext.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/skia_util.h"

WebViewColorOverlay::WebViewColorOverlay(content::RenderView* render_view,
                                         SkColor color)
    : render_view_(render_view),
      color_(color) {
  render_view_->GetWebView()->addPageOverlay(this, 0);
}

WebViewColorOverlay::~WebViewColorOverlay() {
  if (render_view_->GetWebView())
    render_view_->GetWebView()->removePageOverlay(this);
}

void WebViewColorOverlay::paintPageOverlay(blink::WebGraphicsContext* context,
                                           const blink::WebSize& webViewSize) {
  gfx::Rect rect(webViewSize);
  SkCanvas* canvas = context->beginDrawing(gfx::RectF(rect));
  SkPaint paint;
  paint.setColor(color_);
  paint.setStyle(SkPaint::kFill_Style);
  canvas->drawRect(gfx::RectToSkRect(rect), paint);
  context->endDrawing();
}

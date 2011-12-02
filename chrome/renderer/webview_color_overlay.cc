// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/webview_color_overlay.h"

#include "base/logging.h"
#include "content/public/renderer/render_view.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/gfx/size.h"
#include "ui/gfx/skia_util.h"

#if WEBKIT_USING_CG
#include "skia/ext/skia_utils_mac.h"
#endif

WebViewColorOverlay::WebViewColorOverlay(content::RenderView* render_view,
                                         SkColor color)
    : render_view_(render_view),
      color_(color) {
  render_view_->GetWebView()->addPageOverlay(this, 0);
}

WebViewColorOverlay::~WebViewColorOverlay() {
  render_view_->GetWebView()->removePageOverlay(this);
}

void WebViewColorOverlay::paintPageOverlay(WebKit::WebCanvas* canvas) {
  SkRect rect = gfx::RectToSkRect(gfx::Rect(render_view_->GetSize()));

#if WEBKIT_USING_SKIA
  SkPaint paint;
  paint.setColor(color_);
  paint.setStyle(SkPaint::kFill_Style);
  canvas->drawRect(rect, paint);
#elif WEBKIT_USING_CG
  CGContextSaveGState(canvas);
  CGColorRef color = gfx::SkColorToCGColorRef(color_);
  CGContextSetFillColorWithColor(canvas, color);
  CGColorRelease(color);
  CGContextFillRect(canvas, gfx::SkRectToCGRect(rect));
  CGContextRestoreGState(canvas);
#else
  NOTIMPLEMENTED();
#endif
}

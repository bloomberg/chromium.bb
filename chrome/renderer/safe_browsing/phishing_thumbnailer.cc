// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/safe_browsing/phishing_thumbnailer.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/time.h"
#include "content/public/renderer/render_view.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/gfx/size.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebRect;
using WebKit::WebSize;
using WebKit::WebView;

namespace safe_browsing {

SkBitmap GrabPhishingThumbnail(content::RenderView* render_view,
                               const gfx::Size& view_size,
                               const gfx::Size& thumbnail_size) {
  if (!render_view || !render_view->GetWebView()) {
    return SkBitmap();  // The WebView is going away.
  }
  WebView* view = render_view->GetWebView();
  base::TimeTicks beginning_time = base::TimeTicks::Now();
  skia::PlatformCanvas canvas;
  if (!canvas.initialize(view_size.width(), view_size.height(), true)) {
    return SkBitmap();
  }

  // Make sure we are not using any zoom when we take the snapshot.  We will
  // restore the previous zoom level after the snapshot is taken.
  double old_zoom_level = view->zoomLevel();
  if (view->zoomLevel() != 0.0) {
    view->setZoomLevel(false, 0.0);
  }
  WebSize old_size = view->size();
  // TODO(noelutz): not only should we hide all scroll bars but we should also
  // make sure that all scroll-bars are at the top.
  view->mainFrame()->setCanHaveScrollbars(false);  // always hide scrollbars.
  view->resize(view_size);
  view->layout();
  view->paint(webkit_glue::ToWebCanvas(&canvas),
              WebRect(0, 0, view_size.width(), view_size.height()));

  SkDevice* device = skia::GetTopDevice(canvas);

  // Now resize the thumbnail to the right size.  Note: it is important that we
  // use this resize algorithm here.
  const SkBitmap& bitmap = device->accessBitmap(false);
  SkBitmap thumbnail = skia::ImageOperations::Resize(
      bitmap,
      skia::ImageOperations::RESIZE_LANCZOS3,
      thumbnail_size.width(),
      thumbnail_size.height());

  // Put things back as they were before.
  if (view->zoomLevel() != old_zoom_level) {
    view->setZoomLevel(false, old_zoom_level);
  }
  // Maybe re-display the scrollbars and resize the view to its old size.
  view->mainFrame()->setCanHaveScrollbars(
      render_view->ShouldDisplayScrollbars(old_size.width, old_size.height));
  view->resize(old_size);

  UMA_HISTOGRAM_TIMES("SBClientPhishing.GrabPhishingThumbnail",
                      base::TimeTicks::Now() - beginning_time);
  return thumbnail;
}

}  // namespace safe_browsing

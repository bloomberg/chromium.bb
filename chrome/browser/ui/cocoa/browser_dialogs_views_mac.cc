// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/browser_dialogs_views_mac.h"

#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"

// This file provides definitions of desktop browser dialog-creation methods for
// Mac where a Cocoa browser is using Views dialogs. I.e. it is included in the
// Cocoa build and definitions under chrome/browser/ui/cocoa may select at
// runtime whether to show a Cocoa dialog, or the toolkit-views dialog defined
// here (declared in browser_dialogs.h).

namespace chrome {

void CloseZoomBubbleViews() {
  ZoomBubbleView::CloseCurrentBubble();
}

void RefreshZoomBubbleViews() {
  if (ZoomBubbleView::GetZoomBubble())
    ZoomBubbleView::GetZoomBubble()->Refresh();
}

bool IsZoomBubbleViewsShown() {
  return ZoomBubbleView::GetZoomBubble() != nullptr;
}

}  // namespace chrome

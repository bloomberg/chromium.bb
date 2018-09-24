// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble_anchor_util_views.h"

#import <Cocoa/Cocoa.h>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/cocoa/browser_window_views_mac.h"
#import "chrome/browser/ui/cocoa/bubble_anchor_helper.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#include "chrome/browser/ui/extensions/extension_installed_bubble.h"
#import "ui/base/cocoa/cocoa_base_utils.h"
#include "ui/base/ui_features.h"
#include "ui/gfx/geometry/rect.h"
#import "ui/gfx/mac/coordinate_conversion.h"

// This file contains the bubble_anchor_util implementation for
// BrowserWindowCocoa.

namespace bubble_anchor_util {

gfx::Rect GetAppMenuAnchorRectCocoa(Browser* browser) {
  NSWindow* window = browser->window()->GetNativeWindow();
  BrowserWindowController* bwc = BrowserWindowControllerForWindow(window);
  NSPoint point = [[bwc toolbarController] appMenuBubblePoint];
  return gfx::Rect(gfx::ScreenPointFromNSPoint(
                       ui::ConvertPointFromWindowToScreen(window, point)),
                   gfx::Size());
}

}  // namespace bubble_anchor_util

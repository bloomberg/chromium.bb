// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/bubble_anchor_helper.h"

#import <Cocoa/Cocoa.h>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/bubble_anchor_util.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_controller.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#import "ui/base/cocoa/cocoa_base_utils.h"

bool HasVisibleLocationBarForBrowser(Browser* browser) {
  if (!browser->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR))
    return false;

  if (!browser->exclusive_access_manager()->context()->IsFullscreen())
    return true;

  // Return false only if the toolbar is fully hidden.
  BrowserWindowController* bwc = [BrowserWindowController
      browserWindowControllerForWindow:browser->window()->GetNativeWindow()];
  return [[bwc fullscreenToolbarController] toolbarFraction] != 0;
}

NSPoint GetPageInfoAnchorPointForBrowser(Browser* browser) {
  return GetPageInfoAnchorPointForBrowser(
      browser, HasVisibleLocationBarForBrowser(browser));
}

NSPoint GetPageInfoAnchorPointForBrowser(Browser* browser,
                                         bool has_location_bar) {
  NSPoint anchor;
  NSWindow* parentWindow = browser->window()->GetNativeWindow();
  if (has_location_bar) {
    LocationBarViewMac* location_bar =
        [[parentWindow windowController] locationBarBridge];
    anchor = location_bar->GetPageInfoBubblePoint();
  } else {
    // Position the bubble on the left of the screen if there is no page info
    // button to point at.
    NSRect contentFrame = [[parentWindow contentView] frame];
    anchor = NSMakePoint(
        NSMinX(contentFrame) + bubble_anchor_util::kNoToolbarLeftOffset,
        NSMaxY(contentFrame));
  }

  return ui::ConvertPointFromWindowToScreen(parentWindow, anchor);
}

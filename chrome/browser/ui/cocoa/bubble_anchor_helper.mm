// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/bubble_anchor_helper.h"

#import <Cocoa/Cocoa.h>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#import "ui/base/cocoa/cocoa_base_utils.h"

namespace {

// Offset from the screen edge to show dialogs when there is no location bar.
// Don't center, since that could obscure a fullscreen bubble.
constexpr NSInteger kFullscreenLeftOffset = 40;

}  // namespace

bool HasVisibleLocationBarForBrowser(Browser* browser) {
  if (!browser->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR))
    return false;

  if (!browser->exclusive_access_manager()->context()->IsFullscreen())
    return true;

  // If the browser is in browser-initiated full screen, a preference can cause
  // the toolbar to be hidden.
  if (browser->exclusive_access_manager()
          ->fullscreen_controller()
          ->IsFullscreenForBrowser()) {
    PrefService* prefs = browser->profile()->GetPrefs();
    bool show_toolbar = prefs->GetBoolean(prefs::kShowFullscreenToolbar);
    return show_toolbar;
  }

  // Otherwise this is fullscreen without a toolbar, so there is no visible
  // location bar.
  return false;
}

NSPoint GetPermissionBubbleAnchorPointForBrowser(Browser* browser,
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
    anchor = NSMakePoint(NSMinX(contentFrame) + kFullscreenLeftOffset,
                         NSMaxY(contentFrame));
  }

  return ui::ConvertPointFromWindowToScreen(parentWindow, anchor);
}

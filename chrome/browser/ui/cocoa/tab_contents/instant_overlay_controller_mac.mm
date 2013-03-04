// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/tab_contents/instant_overlay_controller_mac.h"
#include "chrome/browser/instant/instant_overlay_model.h"
#include "chrome/browser/ui/browser.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/tab_contents/overlayable_contents_controller.h"

InstantOverlayControllerMac::InstantOverlayControllerMac(
    Browser* browser,
    BrowserWindowController* window,
    OverlayableContentsController* overlay)
    : InstantOverlayController(browser),
      window_(window),
      overlay_(overlay) {
}

InstantOverlayControllerMac::~InstantOverlayControllerMac() {
}

void InstantOverlayControllerMac::OverlayStateChanged(
    const InstantOverlayModel& model) {
  if (model.mode().is_ntp() || model.mode().is_search_suggestions()) {
    // Drop shadow is only needed if search mode is not |NTP| and overlay does
    // not fill up the entire contents page.
    BOOL drawDropShadow = !model.mode().is_ntp() &&
        !(model.height() == 100 &&
          model.height_units() == INSTANT_SIZE_PERCENT);
    [overlay_ setOverlay:model.GetOverlayContents()
                  height:model.height()
             heightUnits:model.height_units()
          drawDropShadow:drawDropShadow];
  } else {
    [overlay_ setOverlay:NULL
                  height:0
             heightUnits:INSTANT_SIZE_PIXELS
          drawDropShadow:NO];
  }
  browser_->MaybeUpdateBookmarkBarStateForInstantOverlay(model.mode());
  [window_ updateBookmarkBarStateForInstantOverlay];
}

// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/tab_contents/instant_overlay_controller_mac.h"

#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/cocoa/tab_contents/instant_overlay_controller_mac.h"
#import "chrome/browser/ui/cocoa/tab_contents/overlayable_contents_controller.h"
#include "chrome/browser/ui/search/instant_overlay_model.h"
#include "chrome/browser/ui/search/search_model.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

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
  bool set_top_bars_visibility = chrome::search::IsInstantExtendedAPIEnabled();

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
  } else if ([overlay_ isShowingOverlay]) {
    [overlay_ setOverlay:NULL
                  height:0
             heightUnits:INSTANT_SIZE_PIXELS
          drawDropShadow:NO];
  } else {
    set_top_bars_visibility = false;
  }

  if (set_top_bars_visibility) {
    // Set top bars (bookmark and info bars) visibility for current tab via
    // |SearchTabHelper| of current active web contents: top bars are hidden if
    // there's overlay.
    chrome::search::SearchTabHelper* search_tab_helper =
        chrome::search::SearchTabHelper::FromWebContents(
            browser_->tab_strip_model()->GetActiveWebContents());
    if (search_tab_helper) {
      search_tab_helper->model()->SetTopBarsVisible(
          ![overlay_ isShowingOverlay]);
    }
  }

  [window_ updateBookmarkBarStateForInstantOverlay];
}

// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/instant_overlay_controller_views.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/search/instant_overlay_model.h"
#include "chrome/browser/ui/search/search_model.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_container.h"
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "ui/views/controls/webview/webview.h"

InstantOverlayControllerViews::InstantOverlayControllerViews(
    Browser* browser,
    ContentsContainer* contents)
    : InstantOverlayController(browser),
      contents_(contents) {
}

InstantOverlayControllerViews::~InstantOverlayControllerViews() {
}

void InstantOverlayControllerViews::OverlayStateChanged(
    const InstantOverlayModel& model) {
  // Set top bars (bookmark and info bars) visibility if Instant Extended API
  // is enabled.
  bool set_top_bars_visibility = chrome::IsInstantExtendedAPIEnabled();

  if (model.mode().is_ntp() || model.mode().is_search_suggestions()) {
    // Show the overlay.
    if (!overlay_) {
      overlay_.reset(new views::WebView(browser_->profile()));
      overlay_->set_id(VIEW_ID_TAB_CONTAINER);
    }
    // Drop shadow is only needed if search mode is not |NTP| and overlay does
    // not fill up the entire contents page.
    bool draw_drop_shadow = !model.mode().is_ntp() &&
        !(contents_->IsOverlayFullHeight(model.height(), model.height_units()));
    content::WebContents* web_contents = model.GetOverlayContents();
    contents_->SetOverlay(overlay_.get(), web_contents, model.height(),
                          model.height_units(), draw_drop_shadow);
    overlay_->SetWebContents(web_contents);
  } else if (overlay_) {
    // Hide the overlay. SetWebContents() must happen before SetOverlay().
    overlay_->SetWebContents(NULL);
    contents_->SetOverlay(NULL, NULL, 100, INSTANT_SIZE_PERCENT, false);
    overlay_.reset();
  } else {
    // Don't set top bars visiblility if nothing was done with overlay.
    set_top_bars_visibility = false;
  }

  if (set_top_bars_visibility) {
    // Set top bars visibility for current tab via |SearchTabHelper| of current
    // active web contents: top bars are hidden if there's overlay.
    SearchTabHelper* search_tab_helper = SearchTabHelper::FromWebContents(
        browser_->tab_strip_model()->GetActiveWebContents());
    if (search_tab_helper)
      search_tab_helper->model()->SetTopBarsVisible(!overlay_);
  }

  // If an Instant overlay is added during an immersive mode reveal, the reveal
  // view needs to stay on top.
  if (overlay_) {
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
    if (browser_view)
      browser_view->MaybeStackImmersiveRevealAtTop();
  }
}

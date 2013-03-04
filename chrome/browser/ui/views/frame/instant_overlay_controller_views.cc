// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/instant_overlay_controller_views.h"

#include "chrome/browser/instant/instant_overlay_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
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
    contents_->SetOverlay(overlay_.get(), web_contents, model.mode(),
                          model.height(), model.height_units(),
                          draw_drop_shadow);
    overlay_->SetWebContents(web_contents);
  } else if (overlay_) {
    // Hide the overlay. SetWebContents() must happen before SetOverlay().
    overlay_->SetWebContents(NULL);
    contents_->SetOverlay(NULL, NULL, model.mode(), 100, INSTANT_SIZE_PERCENT,
                          false);
    overlay_.reset();
  }

  browser_->MaybeUpdateBookmarkBarStateForInstantOverlay(model.mode());

  // If an Instant overlay is added during an immersive mode reveal, the reveal
  // view needs to stay on top.
  // Notify infobar container of change in overlay state.
  if (overlay_) {
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
    if (browser_view) {
      browser_view->MaybeStackImmersiveRevealAtTop();
      browser_view->infobar_container()->OverlayStateChanged(model);
    }
  }
}

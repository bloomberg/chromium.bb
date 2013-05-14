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
#include "chrome/browser/ui/views/frame/overlay_container.h"
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "ui/views/controls/webview/webview.h"

InstantOverlayControllerViews::InstantOverlayControllerViews(
    Browser* browser,
    OverlayContainer* overlay_container)
    : InstantOverlayController(browser),
      overlay_container_(overlay_container) {
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
        !(overlay_container_->WillOverlayBeFullHeight(model.height(),
                                                      model.height_units()));
    content::WebContents* web_contents = model.GetOverlayContents();
    overlay_container_->SetOverlay(overlay_.get(),
                                   web_contents,
                                   model.height(),
                                   model.height_units(),
                                   draw_drop_shadow);
    overlay_->SetWebContents(web_contents);
  } else if (overlay_) {
    // Hide the overlay. SetWebContents() must happen before SetOverlay().
    overlay_->SetWebContents(NULL);
    overlay_container_->SetOverlay(
        NULL, NULL, 100, INSTANT_SIZE_PERCENT, false);
    overlay_.reset();
  }
}

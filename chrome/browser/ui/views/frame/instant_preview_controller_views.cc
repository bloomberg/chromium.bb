// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/instant_preview_controller_views.h"

#include "chrome/browser/instant/instant_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_container.h"
#include "ui/views/controls/webview/webview.h"

InstantPreviewControllerViews::InstantPreviewControllerViews(
    Browser* browser,
    ContentsContainer* contents)
    : InstantPreviewController(browser),
      contents_(contents) {
}

InstantPreviewControllerViews::~InstantPreviewControllerViews() {
}

void InstantPreviewControllerViews::PreviewStateChanged(
    const InstantModel& model) {
  if (model.mode().is_ntp() || model.mode().is_search_suggestions()) {
    // Show the preview.
    if (!preview_) {
      preview_.reset(new views::WebView(browser_->profile()));
      preview_->set_id(VIEW_ID_TAB_CONTAINER);
    }
    // Drop shadow is only needed if search mode is not |NTP| and preview does
    // not fill up the entire contents page.
    bool draw_drop_shadow = !model.mode().is_ntp() &&
        !(contents_->IsPreviewFullHeight(model.height(), model.height_units()));
    content::WebContents* web_contents = model.GetPreviewContents();
    contents_->SetPreview(preview_.get(), web_contents, model.mode(),
                          model.height(), model.height_units(),
                          draw_drop_shadow);
    preview_->SetWebContents(web_contents);
  } else if (preview_) {
    // Hide the preview. SetWebContents() must happen before SetPreview().
    preview_->SetWebContents(NULL);
    contents_->SetPreview(NULL, NULL, model.mode(), 100, INSTANT_SIZE_PERCENT,
                          false);
    preview_.reset();
  }

  browser_->MaybeUpdateBookmarkBarStateForInstantPreview(model.mode());

  // If an instant preview is added during an immersive mode reveal, the reveal
  // view needs to stay on top.
  if (preview_) {
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
    if (browser_view)
      browser_view->MaybeStackImmersiveRevealAtTop();
  }
}

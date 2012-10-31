// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/instant_preview_controller_views.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_container.h"
#include "chrome/browser/instant/instant_model.h"
#include "ui/views/controls/webview/webview.h"

InstantPreviewControllerViews::InstantPreviewControllerViews(
    Browser* browser,
    BrowserView* browser_view,
    ContentsContainer* contents)
    : InstantPreviewController(browser),
      browser_view_(browser_view),
      contents_(contents),
      preview_container_(NULL) {
}

InstantPreviewControllerViews::~InstantPreviewControllerViews() {
}

void InstantPreviewControllerViews::DisplayStateChanged(
    const InstantModel& model) {
  if (model.is_ready()) {
    ShowInstant(model.GetPreviewContents(),
                model.height(), model.height_units());
  } else {
    HideInstant();
  }
}

void InstantPreviewControllerViews::ShowInstant(TabContents* preview,
                                                int height,
                                                InstantSizeUnits units) {
  if (!preview_container_) {
    preview_container_ = new views::WebView(browser_->profile());
    preview_container_->set_id(VIEW_ID_TAB_CONTAINER);
  }
  contents_->SetPreview(preview_container_,
                        preview->web_contents(),
                        height, units);
  preview_container_->SetWebContents(preview->web_contents());
}

void InstantPreviewControllerViews::HideInstant() {
  if (!preview_container_)
    return;

  // The contents must be changed before SetPreview is invoked.
  preview_container_->SetWebContents(NULL);
  contents_->SetPreview(NULL, NULL, 100, INSTANT_SIZE_PERCENT);
  delete preview_container_;
  preview_container_ = NULL;
}

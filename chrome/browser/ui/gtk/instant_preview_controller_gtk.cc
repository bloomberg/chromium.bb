// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/instant_preview_controller_gtk.h"

#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/tab_contents_container_gtk.h"
#include "chrome/browser/instant/instant_model.h"

InstantPreviewControllerGtk::InstantPreviewControllerGtk(
    Browser* browser,
    BrowserWindowGtk* window,
    TabContentsContainerGtk* contents)
    : InstantPreviewController(browser),
      window_(window),
      contents_(contents) {
}

InstantPreviewControllerGtk::~InstantPreviewControllerGtk() {
}

void InstantPreviewControllerGtk::DisplayStateChanged(
    const InstantModel& model) {
  if (model.is_ready()) {
    ShowInstant(model.GetPreviewContents(),
                model.height(), model.height_units());
  } else {
    HideInstant();
  }
}

void InstantPreviewControllerGtk::ShowInstant(TabContents* preview,
                                              int height,
                                              InstantSizeUnits units) {
  // TODO(jered): Support height < 100%.
  DCHECK(height == 100 && units == INSTANT_SIZE_PERCENT);
  contents_->SetPreview(preview);
  window_->MaybeShowBookmarkBar(false);
}

void InstantPreviewControllerGtk::HideInstant() {
  contents_->SetPreview(NULL);
  window_->MaybeShowBookmarkBar(false);
}

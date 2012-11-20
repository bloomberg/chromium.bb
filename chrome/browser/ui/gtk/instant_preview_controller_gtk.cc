// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/instant_preview_controller_gtk.h"

#include "chrome/browser/instant/instant_model.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/tab_contents_container_gtk.h"

InstantPreviewControllerGtk::InstantPreviewControllerGtk(
    BrowserWindowGtk* window,
    TabContentsContainerGtk* contents)
    : InstantPreviewController(window->browser()),
      window_(window),
      contents_(contents) {
}

InstantPreviewControllerGtk::~InstantPreviewControllerGtk() {
}

void InstantPreviewControllerGtk::PreviewStateChanged(
    const InstantModel& model) {
  if (model.mode().is_search_suggestions()) {
    // TODO(jered): Support non-100% height.
    contents_->SetPreview(model.GetPreviewContents());
  } else {
    contents_->SetPreview(NULL);
  }
  window_->MaybeShowBookmarkBar(false);
}

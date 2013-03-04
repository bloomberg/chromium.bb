// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/instant_overlay_controller_gtk.h"

#include "chrome/browser/instant/instant_overlay_model.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/tab_contents_container_gtk.h"

InstantOverlayControllerGtk::InstantOverlayControllerGtk(
    BrowserWindowGtk* window,
    TabContentsContainerGtk* contents)
    : InstantOverlayController(window->browser()),
      window_(window),
      contents_(contents) {
}

InstantOverlayControllerGtk::~InstantOverlayControllerGtk() {
}

void InstantOverlayControllerGtk::OverlayStateChanged(
    const InstantOverlayModel& model) {
  if (model.mode().is_search_suggestions()) {
    // TODO(jered): Support non-100% height.
    contents_->SetOverlay(model.GetOverlayContents());
  } else {
    contents_->SetOverlay(NULL);
  }
  window_->MaybeShowBookmarkBar(false);
}

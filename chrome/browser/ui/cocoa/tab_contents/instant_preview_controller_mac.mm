// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/tab_contents/instant_preview_controller_mac.h"

#include "chrome/browser/instant/instant_model.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/tab_contents/previewable_contents_controller.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"

InstantPreviewControllerMac::InstantPreviewControllerMac(
    Browser* browser,
    BrowserWindowController* window_controller,
    PreviewableContentsController* previewable_contents_controller)
    : InstantPreviewController(browser),
      window_controller_(window_controller),
      previewable_contents_controller_(previewable_contents_controller) {
}

InstantPreviewControllerMac::~InstantPreviewControllerMac() {
}

void InstantPreviewControllerMac::DisplayStateChanged(
    const InstantModel& model) {
  if (model.is_ready()) {
    // TODO(dhollowa): Needs height and units implementation on Mac.
    [previewable_contents_controller_
        showPreview:model.GetPreviewContents()->web_contents()];
  } else {
    if (![previewable_contents_controller_ isShowingPreview])
      return;
    [previewable_contents_controller_ hidePreview];
  }
  [window_controller_ updateBookmarkBarVisibilityWithAnimation:NO];
}

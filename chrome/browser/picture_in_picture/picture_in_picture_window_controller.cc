// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/picture_in_picture_window_controller.h"

#include "chrome/browser/ui/overlay/overlay_window.h"
#include "content/public/browser/web_contents.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(PictureInPictureWindowController);

// static
PictureInPictureWindowController*
PictureInPictureWindowController::GetOrCreateForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);

  // This is a no-op if the controller already exists.
  CreateForWebContents(web_contents);
  return FromWebContents(web_contents);
}

PictureInPictureWindowController::~PictureInPictureWindowController() {
  window_->Close();
}

PictureInPictureWindowController::PictureInPictureWindowController(
    content::WebContents* initiator)
    : initiator_(initiator) {
  DCHECK(initiator_);
}

void PictureInPictureWindowController::Show() {
  if (window_ && window_->IsActive())
    return;

  if (!window_) {
    window_ = OverlayWindow::Create();
    window_->Init();
  }
  window_->Show();
}

void PictureInPictureWindowController::Close() {
  if (window_->IsActive())
    window_->Close();
}

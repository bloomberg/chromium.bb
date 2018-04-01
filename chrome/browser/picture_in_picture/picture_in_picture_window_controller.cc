// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/picture_in_picture_window_controller.h"

#include "chrome/browser/overlay/overlay_surface_embedder.h"
#include "chrome/browser/overlay/overlay_window.h"
#include "components/viz/common/surfaces/surface_id.h"
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
  if (window_)
    window_->Close();
}

PictureInPictureWindowController::PictureInPictureWindowController(
    content::WebContents* initiator)
    : initiator_(initiator) {
  DCHECK(initiator_);
  window_ = OverlayWindow::Create();
}

void PictureInPictureWindowController::Show() {
  DCHECK(window_);
  DCHECK(surface_id_.is_valid());
  window_->Show();
}

void PictureInPictureWindowController::Close() {
  if (window_->IsActive())
    window_->Close();

  surface_id_ = viz::SurfaceId();
}

void PictureInPictureWindowController::EmbedSurface(viz::SurfaceId surface_id) {
  DCHECK(window_);
  DCHECK(surface_id.is_valid());
  surface_id_ = surface_id;

  if (!embedder_)
    embedder_.reset(new OverlaySurfaceEmbedder(window_.get()));
  embedder_->SetPrimarySurfaceId(surface_id_);
}

OverlayWindow* PictureInPictureWindowController::GetWindowForTesting() {
  return window_.get();
}

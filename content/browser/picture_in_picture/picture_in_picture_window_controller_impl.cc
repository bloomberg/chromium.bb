// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/picture_in_picture/picture_in_picture_window_controller_impl.h"

#include "components/viz/common/surfaces/surface_id.h"
#include "content/browser/picture_in_picture/overlay_surface_embedder.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"

namespace content {

DEFINE_WEB_CONTENTS_USER_DATA_KEY(PictureInPictureWindowControllerImpl);

// static
PictureInPictureWindowController*
PictureInPictureWindowController::GetOrCreateForWebContents(
    WebContents* web_contents) {
  return PictureInPictureWindowControllerImpl::GetOrCreateForWebContents(
      web_contents);
}

// static
PictureInPictureWindowControllerImpl*
PictureInPictureWindowControllerImpl::GetOrCreateForWebContents(
    WebContents* web_contents) {
  DCHECK(web_contents);

  // This is a no-op if the controller already exists.
  CreateForWebContents(web_contents);
  return FromWebContents(web_contents);
}

PictureInPictureWindowControllerImpl::~PictureInPictureWindowControllerImpl() {
  if (window_)
    window_->Close();
}

PictureInPictureWindowControllerImpl::PictureInPictureWindowControllerImpl(
    WebContents* initiator) {
  DCHECK(initiator);

  window_ = GetContentClient()->browser()->CreateWindowForPictureInPicture();
  DCHECK(window_) << "Picture in Picture requires a valid window.";
}

void PictureInPictureWindowControllerImpl::Show() {
  DCHECK(window_);
  DCHECK(surface_id_.is_valid());
  window_->Show();
}

void PictureInPictureWindowControllerImpl::Close() {
  if (window_)
    window_->Close();

  surface_id_ = viz::SurfaceId();
}

void PictureInPictureWindowControllerImpl::EmbedSurface(
    const viz::SurfaceId& surface_id,
    const gfx::Size& natural_size) {
  DCHECK(window_);
  DCHECK(surface_id.is_valid());
  surface_id_ = surface_id;

  window_->UpdateVideoSize(natural_size);

  if (!embedder_)
    embedder_.reset(new OverlaySurfaceEmbedder(window_.get()));
  embedder_->SetPrimarySurfaceId(surface_id_);
}

OverlayWindow* PictureInPictureWindowControllerImpl::GetWindowForTesting() {
  return window_.get();
}

}  // namespace content
// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/picture_in_picture/picture_in_picture_window_controller_impl.h"

#include "components/viz/common/surfaces/surface_id.h"
#include "content/browser/media/media_web_contents_observer.h"
#include "content/browser/picture_in_picture/overlay_surface_embedder.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/media/media_player_delegate_messages.h"
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
    WebContents* initiator)
    : initiator_(initiator) {
  DCHECK(initiator_);

  window_ =
      content::GetContentClient()->browser()->CreateWindowForPictureInPicture(
          this);
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

void PictureInPictureWindowControllerImpl::TogglePlayPause() {
  DCHECK(window_ && window_->IsActive());

  content::MediaWebContentsObserver* observer =
      static_cast<content::WebContentsImpl* const>(initiator_)
          ->media_web_contents_observer();
  base::Optional<content::WebContentsObserver::MediaPlayerId> player_id =
      observer->GetPictureInPictureVideoMediaPlayerId();
  DCHECK(player_id.has_value());

  if (observer->IsPlayerActive(*player_id)) {
    player_id->first->Send(new MediaPlayerDelegateMsg_Pause(
        player_id->first->GetRoutingID(), player_id->second));
  } else {
    player_id->first->Send(new MediaPlayerDelegateMsg_Play(
        player_id->first->GetRoutingID(), player_id->second));
  }
}

}  // namespace content
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

  // If the initiator WebContents is being destroyed, there is no need to put
  // the video's media player in a post-Picture-in-Picture mode. In fact, some
  // things, such as the MediaWebContentsObserver, may already been torn down.
  if (initiator_->IsBeingDestroyed())
    return;

  // |this| is torn down when there is a new Picture-in-Picture initiator, such
  // as when a video in another tab requests to enter Picture-in-Picture. In
  // cases like this, pause the current video so there is only one video
  // playing at a time.
  if (IsPlayerActive()) {
    media_player_id_->first->Send(new MediaPlayerDelegateMsg_Pause(
        media_player_id_->first->GetRoutingID(), media_player_id_->second));
    media_player_id_->first->Send(
        new MediaPlayerDelegateMsg_EndPictureInPictureMode(
            media_player_id_->first->GetRoutingID(), media_player_id_->second));
  }
}

PictureInPictureWindowControllerImpl::PictureInPictureWindowControllerImpl(
    WebContents* initiator)
    : initiator_(initiator) {
  DCHECK(initiator_);

  media_web_contents_observer_ = static_cast<WebContentsImpl* const>(initiator_)
                                     ->media_web_contents_observer();
  media_player_id_ =
      media_web_contents_observer_->GetPictureInPictureVideoMediaPlayerId();

  window_ =
      GetContentClient()->browser()->CreateWindowForPictureInPicture(this);
  DCHECK(window_) << "Picture in Picture requires a valid window.";
}

void PictureInPictureWindowControllerImpl::Show() {
  DCHECK(window_);
  DCHECK(surface_id_.is_valid());

  window_->Show();
}

void PictureInPictureWindowControllerImpl::Close() {
  DCHECK(window_);
  window_->Hide();

  surface_id_ = viz::SurfaceId();

  if (IsPlayerActive()) {
    media_player_id_->first->Send(
        new MediaPlayerDelegateMsg_EndPictureInPictureMode(
            media_player_id_->first->GetRoutingID(), media_player_id_->second));
  }
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

void PictureInPictureWindowControllerImpl::UpdateLayerBounds() {
  if (embedder_)
    embedder_->UpdateLayerBounds();
}

bool PictureInPictureWindowControllerImpl::IsPlayerActive() {
  if (!media_player_id_.has_value())
    media_web_contents_observer_->GetPictureInPictureVideoMediaPlayerId();

  return media_player_id_.has_value() &&
         media_web_contents_observer_->IsPlayerActive(*media_player_id_);
}

WebContents* PictureInPictureWindowControllerImpl::GetInitiatorWebContents() {
  return initiator_;
}

bool PictureInPictureWindowControllerImpl::TogglePlayPause() {
  DCHECK(window_ && window_->IsActive());

  if (IsPlayerActive()) {
    media_player_id_->first->Send(new MediaPlayerDelegateMsg_Pause(
        media_player_id_->first->GetRoutingID(), media_player_id_->second));
    return false;
  }

  media_player_id_->first->Send(new MediaPlayerDelegateMsg_Play(
      media_player_id_->first->GetRoutingID(), media_player_id_->second));
  return true;
}

}  // namespace content
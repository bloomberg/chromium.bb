// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android_video_surface_chooser_impl.h"

namespace media {

AndroidVideoSurfaceChooserImpl::AndroidVideoSurfaceChooserImpl(
    bool allow_dynamic)
    : allow_dynamic_(allow_dynamic), weak_factory_(this) {}

AndroidVideoSurfaceChooserImpl::~AndroidVideoSurfaceChooserImpl() {}

void AndroidVideoSurfaceChooserImpl::Initialize(
    UseOverlayCB use_overlay_cb,
    UseSurfaceTextureCB use_surface_texture_cb,
    AndroidOverlayFactoryCB initial_factory,
    const State& initial_state) {
  use_overlay_cb_ = std::move(use_overlay_cb);
  use_surface_texture_cb_ = std::move(use_surface_texture_cb);

  overlay_factory_ = std::move(initial_factory);

  current_state_ = initial_state;

  // Pre-M, we choose now.  This lets Choose() never worry about the pre-M path.
  if (!allow_dynamic_) {
    if (overlay_factory_ &&
        (current_state_.is_fullscreen || current_state_.is_secure)) {
      SwitchToOverlay();
    } else {
      SwitchToSurfaceTexture();
    }
  } else {
    Choose();
  }
}

void AndroidVideoSurfaceChooserImpl::UpdateState(
    base::Optional<AndroidOverlayFactoryCB> new_factory,
    const State& new_state) {
  current_state_ = new_state;

  if (!new_factory) {
    if (allow_dynamic_)
      Choose();
    return;
  }

  overlay_factory_ = std::move(*new_factory);

  // If we started construction of an overlay, but it's not ready yet, then
  // just drop it.  It's from the wrong factory.
  if (overlay_)
    overlay_ = nullptr;

  // Pre-M, we can't change the output surface.  Just stop here.
  if (!allow_dynamic_) {
    // If we still haven't told the client anything, then it's because an
    // overlay factory was provided to Initialize(), and changed before the
    // overlay request finished.  Switch to SurfaceTexture.  We could, I guess,
    // try to use the new factory if any.
    if (client_overlay_state_ == kUnknown)
      SwitchToSurfaceTexture();
    return;
  }

  // We're switching factories, so any existing overlay should be cancelled.
  // We also clear the state back to Unknown so that there's no confusion about
  // whether the client is using the 'right' overlay; it's not.  Any previous
  // overlay was from the previous factory.
  if (client_overlay_state_ == kUsingOverlay) {
    overlay_ = nullptr;
    client_overlay_state_ = kUnknown;
  }

  Choose();
}

void AndroidVideoSurfaceChooserImpl::Choose() {
  // Pre-M, we decide based on whether we have or don't have a factory.  We
  // shouldn't be called.
  DCHECK(allow_dynamic_);

  OverlayState new_overlay_state = kUsingSurfaceTexture;

  // In player element fullscreen, we want to use overlays if we can.
  if (current_state_.is_fullscreen)
    new_overlay_state = kUsingOverlay;

  // TODO(liberato): add other checks for things like "safe for overlay".

  // If we need a secure surface, then we must choose an overlay.  The only way
  // we won't is if we don't have a factory.
  if (current_state_.is_secure)
    new_overlay_state = kUsingOverlay;

  // If we have no factory, then we definitely don't want to use overlays.
  if (!overlay_factory_)
    new_overlay_state = kUsingSurfaceTexture;

  // Make sure that we're in |new_overlay_state_|.
  if (new_overlay_state == kUsingSurfaceTexture)
    SwitchToSurfaceTexture();
  else
    SwitchToOverlay();
}

void AndroidVideoSurfaceChooserImpl::SwitchToSurfaceTexture() {
  // Cancel any outstanding overlay request, in case we're switching to overlay.
  if (overlay_)
    overlay_ = nullptr;

  // Notify the client to switch if it's in the wrong state.
  if (client_overlay_state_ != kUsingSurfaceTexture) {
    client_overlay_state_ = kUsingSurfaceTexture;
    use_surface_texture_cb_.Run();
  }
}

void AndroidVideoSurfaceChooserImpl::SwitchToOverlay() {
  // If there's already an overlay request outstanding, then do nothing.  We'll
  // finish switching when it completes.
  if (overlay_)
    return;

  // Do nothing if the client is already using an overlay.  Note that if one
  // changes overlay factories, then this might not be true; an overlay from the
  // old factory is not the same as an overlay from the new factory.  However,
  // we assume that ReplaceOverlayFactory handles that.
  if (client_overlay_state_ == kUsingOverlay)
    return;

  // We don't modify |client_overlay_state_| yet, since we don't call the client
  // back yet.

  AndroidOverlayConfig config;
  // We bind all of our callbacks with weak ptrs, since we don't know how long
  // the client will hold on to overlays.  They could, in principle, show up
  // long after the client is destroyed too, if codec destruction hangs.
  config.ready_cb = base::Bind(&AndroidVideoSurfaceChooserImpl::OnOverlayReady,
                               weak_factory_.GetWeakPtr());
  config.failed_cb =
      base::Bind(&AndroidVideoSurfaceChooserImpl::OnOverlayFailed,
                 weak_factory_.GetWeakPtr());
  // TODO(liberato): where do we get the initial size from?  For CVV, it's
  // set via the natural size, and this is ignored anyway.  The client should
  // provide this.
  config.rect = gfx::Rect(0, 0, 1, 1);
  overlay_ = overlay_factory_.Run(std::move(config));
  if (!overlay_)
    SwitchToSurfaceTexture();

  // We could add a destruction callback here, if we need to find out when the
  // surface has been destroyed.  It might also be good to have a 'overlay has
  // been destroyed' callback from ~AndroidOverlay, since we don't really know
  // how long that will take after SurfaceDestroyed.
}

void AndroidVideoSurfaceChooserImpl::OnOverlayReady(AndroidOverlay* overlay) {
  // |overlay_| is the only overlay for which we haven't gotten a ready callback
  // back yet.
  DCHECK_EQ(overlay, overlay_.get());

  client_overlay_state_ = kUsingOverlay;
  use_overlay_cb_.Run(std::move(overlay_));
}

void AndroidVideoSurfaceChooserImpl::OnOverlayFailed(AndroidOverlay* overlay) {
  // We shouldn't get a failure for any overlay except the incoming one.
  DCHECK_EQ(overlay, overlay_.get());

  overlay_ = nullptr;

  // If the client isn't already using a SurfaceTexture, then switch to it.
  // Note that this covers the case of kUnknown, when we might not have told the
  // client anything yet.  That's important for Initialize, so that a failed
  // overlay request still results in some callback to the client to know what
  // surface to start with.
  SwitchToSurfaceTexture();
}

}  // namespace media

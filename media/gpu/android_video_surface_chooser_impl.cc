// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android_video_surface_chooser_impl.h"

#include "base/memory/ptr_util.h"
#include "base/time/default_tick_clock.h"

namespace media {

// Minimum time that we require after a failed overlay attempt before we'll try
// again for an overlay.
constexpr base::TimeDelta MinimumDelayAfterFailedOverlay =
    base::TimeDelta::FromSeconds(5);

AndroidVideoSurfaceChooserImpl::AndroidVideoSurfaceChooserImpl(
    bool allow_dynamic,
    base::TickClock* tick_clock)
    : allow_dynamic_(allow_dynamic),
      tick_clock_(tick_clock),
      weak_factory_(this) {
  // Use a DefaultTickClock if one wasn't provided.
  if (!tick_clock_) {
    optional_tick_clock_ = base::MakeUnique<base::DefaultTickClock>();
    tick_clock_ = optional_tick_clock_.get();
  }
}

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

  // If the compositor won't promote, then don't.
  if (!current_state_.is_compositor_promotable)
    new_overlay_state = kUsingSurfaceTexture;

  // If we're expecting a relayout, then don't transition to overlay if we're
  // not already in one.  We don't want to transition out, though.  This lets us
  // delay entering on a fullscreen transition until blink relayout is complete.
  // TODO(liberato): Detect this more directly.
  if (current_state_.is_expecting_relayout &&
      client_overlay_state_ != kUsingOverlay)
    new_overlay_state = kUsingSurfaceTexture;

  // If we need a secure surface, then we must choose an overlay.  The only way
  // we won't is if we don't have a factory or our request fails.  If the
  // compositor won't promote, then we still use the overlay, since hopefully
  // it's a temporary restriction.  If we drop the overlay, then playback will
  // fail (L1) or be insecure on SurfaceTexture (L3).  For L3, that's still
  // preferable to failing.
  if (current_state_.is_secure)
    new_overlay_state = kUsingOverlay;

  // If we're requesting an overlay, check that we haven't asked too recently
  // since the last failure.  This includes L1.  We don't bother to check for
  // our current state, since using an overlay would imply that our most recent
  // failure was long ago enough to pass this check earlier.
  if (new_overlay_state == kUsingOverlay) {
    base::TimeDelta time_since_last_failure =
        tick_clock_->NowTicks() - most_recent_overlay_failure_;
    if (time_since_last_failure < MinimumDelayAfterFailedOverlay)
      new_overlay_state = kUsingSurfaceTexture;
  }

  // If our frame is hidden, then don't use overlays.
  if (current_state_.is_frame_hidden)
    new_overlay_state = kUsingSurfaceTexture;

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
  config.rect = current_state_.initial_position;
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
  most_recent_overlay_failure_ = tick_clock_->NowTicks();

  // If the client isn't already using a SurfaceTexture, then switch to it.
  // Note that this covers the case of kUnknown, when we might not have told the
  // client anything yet.  That's important for Initialize, so that a failed
  // overlay request still results in some callback to the client to know what
  // surface to start with.
  SwitchToSurfaceTexture();
}

}  // namespace media

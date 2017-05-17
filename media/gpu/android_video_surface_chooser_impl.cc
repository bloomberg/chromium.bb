// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android_video_surface_chooser_impl.h"

namespace media {

AndroidVideoSurfaceChooserImpl::AndroidVideoSurfaceChooserImpl()
    : weak_factory_(this) {}

AndroidVideoSurfaceChooserImpl::~AndroidVideoSurfaceChooserImpl() {}

void AndroidVideoSurfaceChooserImpl::Initialize(
    UseOverlayCB use_overlay_cb,
    UseSurfaceTextureCB use_surface_texture_cb,
    AndroidOverlayFactoryCB initial_factory) {
  use_overlay_cb_ = std::move(use_overlay_cb);
  use_surface_texture_cb_ = std::move(use_surface_texture_cb);

  if (initial_factory) {
    // We requested an overlay.  Wait to see if it succeeds or fails, since
    // hopefully this will be fast.  On M+, we could ask it to start with a
    // SurfaceTexture either way.  Before M, we can't switch surfaces.  In that
    // case, it's important not to request a SurfaceTexture if we have an
    // overlay pending, so that we know not to transition to SurfaceTexture.
    client_notification_pending_ = true;
    ReplaceOverlayFactory(std::move(initial_factory));
  }

  if (!overlay_) {
    // We haven't requested an overlay yet.  Just ask the client to start with
    // SurfaceTexture now.
    use_surface_texture_cb_.Run();
    return;
  }
}

void AndroidVideoSurfaceChooserImpl::ReplaceOverlayFactory(
    AndroidOverlayFactoryCB factory) {
  // If we have an overlay, then we should transition away from it.  It
  // doesn't matter if we have a new factory or no factory; the old overlay goes
  // with the old factory.

  // Notify the client to transition to SurfaceTexture, which it might already
  // be using.  If |!client_notification_pending_|, then we're still during
  // initial startup, so we don't switch the client.  Otherwise, we'd cause
  // pre-M to break, since we'd start with a SurfaceTexture in all cases.
  if (!client_notification_pending_)
    use_surface_texture_cb_.Run();

  // If we started construction of an overlay, but it's not ready yet, then
  // just drop it.
  if (overlay_)
    overlay_ = nullptr;

  overlay_factory_ = std::move(factory);

  // If we don't have a new factory, then just stop here.
  if (!overlay_factory_)
    return;

  // We just got an overlay factory.  Get an overlay immediately.  This is
  // the right behavior to match what AVDA does with CVV.  For DS, we should
  // probably check to see if it's a good idea, at least on M+.
  // Also note that for pre-M, AVDA depends on this behavior, else it will get
  // a SurfaceTexture then be unable to switch.  Perhaps there should be a
  // pre-M implementation of this class that guarantees that it won't change
  // between the two.
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
  // We could add a destruction callback here, if we need to find out when the
  // surface has been destroyed.  It might also be good to have a 'overlay has
  // been destroyed' callback from ~AndroidOverlay, since we don't really know
  // how long that will take after SurfaceDestroyed.
}

void AndroidVideoSurfaceChooserImpl::OnOverlayReady(AndroidOverlay* overlay) {
  // |overlay_| is the only overlay for which we haven't gotten a ready callback
  // back yet.
  DCHECK_EQ(overlay, overlay_.get());

  // If we haven't sent the client notification yet, we're doing so now.
  client_notification_pending_ = false;

  use_overlay_cb_.Run(std::move(overlay_));
}

void AndroidVideoSurfaceChooserImpl::OnOverlayFailed(AndroidOverlay* overlay) {
  // We shouldn't get a failure for any overlay except the incoming one.
  DCHECK_EQ(overlay, overlay_.get());

  // If we owe the client a notification callback, then send it.
  if (client_notification_pending_) {
    // The overlay that we requested failed, so notify the client to try
    // using SurfaceTexture instead.
    client_notification_pending_ = false;
    use_surface_texture_cb_.Run();
  }

  overlay_ = nullptr;
}

}  // namespace media

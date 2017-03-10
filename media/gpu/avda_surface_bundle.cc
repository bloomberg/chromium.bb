// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/avda_surface_bundle.h"
#include "media/base/android/android_overlay.h"

namespace media {

AVDASurfaceBundle::AVDASurfaceBundle(int surface_id) : surface_id(surface_id) {}

AVDASurfaceBundle::~AVDASurfaceBundle() {
  // Explicitly free the surface first, just to be sure that it's deleted before
  // the SurfaceTexture is.
  surface_texture_surface = gl::ScopedJavaSurface();

  // Also release the back buffers.  We might want to let the consumer do this
  // after it has swapped the back buffer it wants, but we don't.
  if (surface_texture)
    surface_texture->ReleaseSurfaceTexture();
  surface_texture = nullptr;
}

}  // namespace media

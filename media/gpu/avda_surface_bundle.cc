// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/avda_surface_bundle.h"

#include "media/base/android/android_overlay.h"

namespace media {

AVDASurfaceBundle::AVDASurfaceBundle() = default;

AVDASurfaceBundle::AVDASurfaceBundle(std::unique_ptr<AndroidOverlay> overlay)
    : overlay(std::move(overlay)) {}

AVDASurfaceBundle::AVDASurfaceBundle(
    scoped_refptr<SurfaceTextureGLOwner> surface_texture_owner)
    : surface_texture(std::move(surface_texture_owner)),
      surface_texture_surface(surface_texture->CreateJavaSurface()) {}

AVDASurfaceBundle::~AVDASurfaceBundle() {
  // Explicitly free the surface first, just to be sure that it's deleted before
  // the SurfaceTexture is.
  surface_texture_surface = gl::ScopedJavaSurface();

  // Also release the back buffers.
  if (surface_texture) {
    auto task_runner = surface_texture->task_runner();
    if (task_runner->RunsTasksInCurrentSequence()) {
      surface_texture->ReleaseBackBuffers();
    } else {
      task_runner->PostTask(
          FROM_HERE, base::Bind(&SurfaceTextureGLOwner::ReleaseBackBuffers,
                                surface_texture));
    }
  }
}

const base::android::JavaRef<jobject>& AVDASurfaceBundle::GetJavaSurface()
    const {
  if (overlay)
    return overlay->GetJavaSurface();
  else
    return surface_texture_surface.j_surface();
}

}  // namespace media

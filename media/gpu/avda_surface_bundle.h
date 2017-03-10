// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_AVDA_SURFACE_BUNDLE_H_
#define MEDIA_GPU_AVDA_SURFACE_BUNDLE_H_

#include "base/memory/ref_counted.h"
#include "media/base/android/android_overlay.h"
#include "media/base/surface_manager.h"
#include "ui/gl/android/scoped_java_surface.h"
#include "ui/gl/android/surface_texture.h"

namespace media {

// AVDASurfaceBundle is a collection of everything that the producer-side of
// the output surface needs.  In other words, it's the surface, and any
// SurfaceTexture that backs it.  The SurfaceTexture isn't needed directly by
// the producer, but destroying it causes the surface not to work.
//
// The idea is that a reference to this should be kept with the codec, even if
// the codec is sent to another thread.  This will prevent the output surface
// from being destroyed while the codec depends on it.
// While you may send a reference to this to other threads, be sure that it
// doesn't drop the reference there without creating another one.  This has to
// be destroyed on the gpu main thread.
class AVDASurfaceBundle : public base::RefCountedThreadSafe<AVDASurfaceBundle> {
 public:
  explicit AVDASurfaceBundle(int surface_id);

  // The surface that MediaCodec is configured to output to.  This can be either
  // a SurfaceTexture or other Surface provider.
  // TODO(liberato): it would be nice if we had an abstraction that included
  // SurfaceTexture and Overlay, but we don't right now.
  const base::android::JavaRef<jobject>& j_surface() const {
    if (overlay)
      return overlay->GetJavaSurface();
    else
      return surface_texture_surface.j_surface();
  }

  int surface_id = SurfaceManager::kNoSurfaceID;

  // The surface onto which the codec is writing.
  // TODO(liberato): this isn't true if we have an overlay.  the overlay keeps
  // the java surface.
  // gl::ScopedJavaSurface surface;

  // If |overlay| is non-null, then |overlay| owns |surface|.
  std::unique_ptr<AndroidOverlay> overlay;

  // The SurfaceTexture attached to |surface|, or nullptr if |surface| is
  // SurfaceView backed.
  scoped_refptr<gl::SurfaceTexture> surface_texture;

  // If |surface_texture| is not null, then this is the surface for it.
  // TODO(liberato): |surface| is the same thing, since overlays own their own
  // surfaces anyway.
  gl::ScopedJavaSurface surface_texture_surface;

 private:
  ~AVDASurfaceBundle();
  friend class base::RefCountedThreadSafe<AVDASurfaceBundle>;

  DISALLOW_COPY_AND_ASSIGN(AVDASurfaceBundle);
};

}  // namespace media

#endif  // MEDIA_GPU_AVDA_SURFACE_BUNDLE_H_

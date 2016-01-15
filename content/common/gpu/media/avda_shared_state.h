// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_AVDA_SHARED_STATE_H_
#define CONTENT_COMMON_GPU_AVDA_SHARED_STATE_H_

#include "base/synchronization/waitable_event.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/base/android/sdk_media_codec_bridge.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_surface.h"

namespace gfx {
class SurfaceTexture;
}

namespace content {

// Shared state to allow communication between the AVDA and the
// GLImages that configure GL for drawing the frames.
class AVDASharedState : public base::RefCounted<AVDASharedState> {
 public:
  AVDASharedState();

  GLint surface_texture_service_id() const {
    return surface_texture_service_id_;
  }

  // Set the SurfaceTexture's client texture name, which the SurfaceTexture
  // might not know about yet (see surface_texture_is_attached()).
  void set_surface_texture_service_id(GLint id) {
    surface_texture_service_id_ = id;
  }

  // Signal the "frame available" event.  This may be called from any thread.
  void SignalFrameAvailable();

  void WaitForFrameAvailable();

  // Context that the surface texture is bound to, or nullptr if it is not in
  // the attached state.
  gfx::GLContext* context() const { return context_.get(); }

  gfx::GLSurface* surface() const { return surface_.get(); }

  bool surface_texture_is_attached() const {
    return surface_texture_is_attached_;
  }

  // Call this when the SurfaceTexture is attached to a GL context.  This will
  // update surface_texture_is_attached(), and set the context() and surface()
  // to match.
  void did_attach_surface_texture();

 private:
  // Platform gl texture Id for |surface_texture_|.  This will be zero if
  // and only if |texture_owner_| is null.
  // TODO(liberato): This should be GLuint, but we don't seem to have the type.
  GLint surface_texture_service_id_;

  // For signalling OnFrameAvailable().
  base::WaitableEvent frame_available_event_;

  // True if and only if the surface texture is currently attached.
  bool surface_texture_is_attached_;

  // Context and surface that the surface texture is attached to, if it is
  // currently attached.
  scoped_refptr<gfx::GLContext> context_;
  scoped_refptr<gfx::GLSurface> surface_;

 protected:
  virtual ~AVDASharedState();

 private:
  friend class base::RefCounted<AVDASharedState>;
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_AVDA_SHARED_STATE_H_

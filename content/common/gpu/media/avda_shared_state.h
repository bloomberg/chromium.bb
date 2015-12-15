// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_AVDA_SHARED_STATE_H_
#define CONTENT_COMMON_GPU_AVDA_SHARED_STATE_H_

#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "media/base/android/media_codec_bridge.h"
#include "ui/gl/gl_image.h"

namespace gfx {
class SurfaceTexture;
}

namespace content {

// Shared state to allow communication between the AVDA and the
// GLImages that configure GL for drawing the frames.
// TODO(liberato): If the deferred backing strategy owned the service id, then
// the shared state could be removed entirely.  However, I'm not yet sure if
// there's an issue with virtual gl contexts.
class AVDASharedState : public base::RefCounted<AVDASharedState> {
 public:
  AVDASharedState() : surface_texture_service_id_(0) {}

  GLint surface_texture_service_id() const {
    return surface_texture_service_id_;
  }

  void set_surface_texture_service_id(GLint id) {
    surface_texture_service_id_ = id;
  }

 private:
  // Platform gl texture Id for |surface_texture_|.  This will be zero if
  // and only if |texture_owner_| is null.
  GLint surface_texture_service_id_;

 protected:
  virtual ~AVDASharedState() {}

 private:
  friend class base::RefCounted<AVDASharedState>;
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_AVDA_SHARED_STATE_H_

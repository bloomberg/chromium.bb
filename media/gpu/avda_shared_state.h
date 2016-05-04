// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_AVDA_SHARED_STATE_H_
#define MEDIA_GPU_AVDA_SHARED_STATE_H_

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

namespace media {

class AVDACodecImage;
class MediaCodecBridge;

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

  // Iterates over all known codec images and updates the MediaCodec attached to
  // each one.
  void CodecChanged(media::MediaCodecBridge* codec);

  // Methods for finding and updating the AVDACodecImage associated with a given
  // picture buffer id. GetImageForPicture() will return null for unknown ids.
  // Calling SetImageForPicture() with a nullptr will erase the entry.
  void SetImageForPicture(int picture_buffer_id, AVDACodecImage* image);
  AVDACodecImage* GetImageForPicture(int picture_buffer_id) const;

  // TODO(liberato): move the surface texture here and make these calls
  // attach / detach it also.  There are several changes going on in avda
  // concurrently, so I don't want to change that until the dust settles.
  // AVDACodecImage would no longer hold the surface texture.

  // Call this when the SurfaceTexture is attached to a GL context.  This will
  // update surface_texture_is_attached(), and set the context() and surface()
  // to match.
  void DidAttachSurfaceTexture();

  // Call this when the SurfaceTexture is detached from its GL context.  This
  // will cause us to forget the last binding.
  void DidDetachSurfaceTexture();

  // Helper method for coordinating the interactions between
  // MediaCodec::ReleaseOutputBuffer() and WaitForFrameAvailable() when
  // rendering to a SurfaceTexture; this method should never be called when
  // rendering to a SurfaceView.
  //
  // The release of the codec buffer to the surface texture is asynchronous, by
  // using this helper we can attempt to let this process complete in a non
  // blocking fashion before the SurfaceTexture is used.
  //
  // Clients should call this method to release the codec buffer for rendering
  // and then call WaitForFrameAvailable() before using the SurfaceTexture. In
  // the ideal case the SurfaceTexture has already been updated, otherwise the
  // method will wait for a pro-rated amount of time based on elapsed time up
  // to a short deadline.
  //
  // Some devices do not reliably notify frame availability, so we use a very
  // short deadline of only a few milliseconds to avoid indefinite stalls.
  void RenderCodecBufferToSurfaceTexture(media::MediaCodecBridge* codec,
                                         int codec_buffer_index);

 protected:
  virtual ~AVDASharedState();

 private:
  friend class base::RefCounted<AVDASharedState>;

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

  // Maps a picture buffer id to a AVDACodecImage.
  std::map<int, AVDACodecImage*> codec_images_;

  // The time of the last call to RenderCodecBufferToSurfaceTexture(), null if
  // if there has been no last call or WaitForFrameAvailable() has been called
  // since the last call.
  base::TimeTicks release_time_;

  DISALLOW_COPY_AND_ASSIGN(AVDASharedState);
};

}  // namespace media

#endif  // MEDIA_GPU_AVDA_SHARED_STATE_H_

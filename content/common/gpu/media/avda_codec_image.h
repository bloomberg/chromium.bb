// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_AVDA_CODEC_IMAGE_H_
#define CONTENT_COMMON_GPU_MEDIA_AVDA_CODEC_IMAGE_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "content/common/gpu/media/avda_shared_state.h"
#include "gpu/command_buffer/service/gl_stream_texture_image.h"

namespace ui {
class ScopedMakeCurrent;
}

namespace content {

// GLImage that renders MediaCodec buffers to a SurfaceTexture or SurfaceView as
// needed in order to draw them.
class AVDACodecImage : public gpu::gles2::GLStreamTextureImage {
 public:
  AVDACodecImage(int picture_buffer_id,
                 const scoped_refptr<AVDASharedState>& shared_state,
                 media::VideoCodecBridge* codec,
                 const base::WeakPtr<gpu::gles2::GLES2Decoder>& decoder,
                 const scoped_refptr<gfx::SurfaceTexture>& surface_texture);

  // gl::GLImage implementation
  void Destroy(bool have_context) override;
  gfx::Size GetSize() override;
  unsigned GetInternalFormat() override;
  bool BindTexImage(unsigned target) override;
  void ReleaseTexImage(unsigned target) override;
  bool CopyTexImage(unsigned target) override;
  bool CopyTexSubImage(unsigned target,
                       const gfx::Point& offset,
                       const gfx::Rect& rect) override;
  bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                            int z_order,
                            gfx::OverlayTransform transform,
                            const gfx::Rect& bounds_rect,
                            const gfx::RectF& crop_rect) override;
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t process_tracing_id,
                    const std::string& dump_name) override;
  // gpu::gles2::GLStreamTextureMatrix implementation
  void GetTextureMatrix(float xform[16]) override;

  // Decoded buffer index that has the image for us to display.
  void SetMediaCodecBufferIndex(int buffer_index);

  // Return the codec buffer that we will return to the codec, or
  // <0 if there is no such buffer.
  int GetMediaCodecBufferIndex() const;

  // Set the size of the current image.
  void SetSize(const gfx::Size& size);

  // Updates the MediaCodec for this image; clears |codec_buffer_index_|.
  void CodecChanged(media::MediaCodecBridge* codec);

  void SetTexture(gpu::gles2::Texture* texture);

 protected:
  ~AVDACodecImage() override;

 private:
  enum { kInvalidCodecBufferIndex = -1 };

  // Make sure that the surface texture's front buffer is current.  This will
  // save / restore the current context.  It will optionally restore the texture
  // bindings in the surface texture's context, based on |mode|.  This is
  // intended as a hint if we don't need to change contexts.  If we do need to
  // change contexts, then we'll always preserve the texture bindings in the
  // both contexts.  In other words, the caller is telling us whether it's
  // okay to change the binding in the current context.
  enum RestoreBindingsMode { kDontRestoreBindings, kDoRestoreBindings };
  void UpdateSurfaceTexture(RestoreBindingsMode mode);

  // Attach the surface texture to our GL context to whatever texture is bound
  // on the active unit.
  void AttachSurfaceTextureToContext();

  // Make shared_state_->context() current if it isn't already.
  std::unique_ptr<ui::ScopedMakeCurrent> MakeCurrentIfNeeded();

  // Return whether or not the current context is in the same share group as
  // |surface_texture_|'s client texture.
  // TODO(liberato): is this needed?
  bool IsCorrectShareGroup() const;

  // Return whether there is a codec buffer that we haven't rendered yet.  Will
  // return false also if there's no codec or we otherwise can't update.
  bool IsCodecBufferOutstanding() const;

  // Shared state between the AVDA and all AVDACodecImages.
  scoped_refptr<AVDASharedState> shared_state_;

  // The MediaCodec buffer index that we should render. Only valid if not equal
  // to |kInvalidCodecBufferIndex|.
  int codec_buffer_index_;

  // Our image size.
  gfx::Size size_;

  // May be null.
  media::MediaCodecBridge* media_codec_;

  const base::WeakPtr<gpu::gles2::GLES2Decoder> decoder_;

  // The SurfaceTexture to render to. This is null when rendering to a
  // SurfaceView.
  const scoped_refptr<gfx::SurfaceTexture> surface_texture_;

  // Should we detach |surface_texture_| from its GL context when we are
  // deleted?  This happens when it's using our Texture's texture handle.
  bool detach_surface_texture_on_destruction_;

  // The texture that we're attached to.
  gpu::gles2::Texture* texture_;

  // Texture matrix of the front buffer of the surface texture.
  float gl_matrix_[16];

  // The picture buffer id attached to this image.
  int picture_buffer_id_;

  DISALLOW_COPY_AND_ASSIGN(AVDACodecImage);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_AVDA_CODEC_IMAGE_H_

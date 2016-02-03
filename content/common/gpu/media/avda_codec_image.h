// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_AVDA_CODEC_IMAGE_H_
#define CONTENT_COMMON_GPU_MEDIA_AVDA_CODEC_IMAGE_H_

#include <stdint.h>

#include "base/macros.h"
#include "content/common/gpu/media/avda_shared_state.h"
#include "ui/gl/gl_image.h"

namespace content {

// GLImage that renders MediaCodec buffers to a SurfaceTexture or SurfaceView as
// needed in order to draw them.
class AVDACodecImage : public gl::GLImage {
 public:
  AVDACodecImage(const scoped_refptr<AVDASharedState>&,
                 media::VideoCodecBridge* codec,
                 const base::WeakPtr<gpu::gles2::GLES2Decoder>& decoder,
                 const scoped_refptr<gfx::SurfaceTexture>& surface_texture);

 protected:
  ~AVDACodecImage() override;

 public:
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

 public:
  // Decoded buffer index that has the image for us to display.
  void SetMediaCodecBufferIndex(int buffer_index);

  // Return the codec buffer that we will return to the codec, or
  // <0 if there is no such buffer.
  int GetMediaCodecBufferIndex() const;

  // Set the size of the current image.
  void SetSize(const gfx::Size& size);

  void SetMediaCodec(media::MediaCodecBridge* codec);

  void SetTexture(gpu::gles2::Texture* texture);

 private:
  enum { kInvalidCodecBufferIndex = -1 };

  // Make sure that the surface texture's front buffer is current.
  void UpdateSurfaceTexture();

  // Attach the surface texture to our GL context, with a texture that we
  // create for it.
  void AttachSurfaceTextureToContext();

  // Install the current texture matrix into the shader.
  void InstallTextureMatrix();

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

  // Have we cached |texmatrix_uniform_location_| yet?
  bool need_shader_info_;

  // Uniform ID of the texture matrix in the shader.
  GLint texmatrix_uniform_location_;

  // Texture matrix of the front buffer of the surface texture.
  float gl_matrix_[16];

  DISALLOW_COPY_AND_ASSIGN(AVDACodecImage);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_AVDA_CODEC_IMAGE_H_

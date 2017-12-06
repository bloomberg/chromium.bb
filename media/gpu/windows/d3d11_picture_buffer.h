// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D11_PICTURE_BUFFER_H_
#define MEDIA_GPU_WINDOWS_D3D11_PICTURE_BUFFER_H_

#include <d3d11.h>
#include <dxva.h>
#include <wrl/client.h>

#include <vector>

#include "gpu/command_buffer/service/texture_manager.h"
#include "media/base/video_frame.h"
#include "media/video/picture.h"

namespace media {
class D3D11H264Accelerator;

// This must be freed on the main thread, since it has things like |gl_image_|
// and |texture_refs_|.
class D3D11PictureBuffer {
 public:
  using MailboxHolderArray = gpu::MailboxHolder[VideoFrame::kMaxPlanes];

  D3D11PictureBuffer(PictureBuffer picture_buffer, size_t level);
  D3D11PictureBuffer(
      PictureBuffer picture_buffer,
      size_t level,
      const std::vector<scoped_refptr<gpu::gles2::TextureRef>>& texture_refs,
      const MailboxHolderArray& mailbox_holders);
  ~D3D11PictureBuffer();

  bool Init(Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device,
            Microsoft::WRL::ComPtr<ID3D11Texture2D> texture,
            const GUID& decoder_guid);

  size_t level() const { return level_; }

  PictureBuffer& picture_buffer() { return picture_buffer_; }

  bool in_client_use() const { return in_client_use_; }
  bool in_picture_use() const { return in_picture_use_; }
  void set_in_client_use(bool use) { in_client_use_ = use; }
  void set_in_picture_use(bool use) { in_picture_use_ = use; }
  scoped_refptr<gl::GLImage> gl_image() const { return gl_image_; }

  // For D3D11VideoDecoder.
  const MailboxHolderArray& mailbox_holders() const { return mailbox_holders_; }
  // Shouldn't be here, but simpler for now.
  base::TimeDelta timestamp_;

 private:
  friend class D3D11H264Accelerator;

  PictureBuffer picture_buffer_;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_;
  bool in_picture_use_ = false;
  bool in_client_use_ = false;
  size_t level_;
  Microsoft::WRL::ComPtr<ID3D11VideoDecoderOutputView> output_view_;
  EGLStreamKHR stream_;
  scoped_refptr<gl::GLImage> gl_image_;

  // For D3D11VideoDecoder.
  std::vector<scoped_refptr<gpu::gles2::TextureRef>> texture_refs_;
  MailboxHolderArray mailbox_holders_;

  DISALLOW_COPY_AND_ASSIGN(D3D11PictureBuffer);
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D11_PICTURE_BUFFER_H_

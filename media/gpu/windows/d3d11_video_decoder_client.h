// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D11_VIDEO_DECODER_CLIENT_H_
#define MEDIA_GPU_WINDOWS_D3D11_VIDEO_DECODER_CLIENT_H_

#include "media/base/video_color_space.h"

namespace media {

class D3D11PictureBuffer;

// Acts as a parent class for the D3D11VideoDecoder to expose
// required methods to D3D11VideoAccelerators.
class D3D11VideoDecoderClient {
 public:
  virtual D3D11PictureBuffer* GetPicture() = 0;
  virtual void OutputResult(D3D11PictureBuffer* picture,
                            const VideoColorSpace& buffer_colorspace) = 0;

 protected:
  virtual ~D3D11VideoDecoderClient() = default;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D11_VIDEO_DECODER_CLIENT_H_

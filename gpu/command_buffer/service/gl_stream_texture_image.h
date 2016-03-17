// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GL_STREAM_TEXTURE_IMAGE_H_
#define GPU_COMMAND_BUFFER_SERVICE_GL_STREAM_TEXTURE_IMAGE_H_

#include "gpu/gpu_export.h"
#include "ui/gl/gl_image.h"

namespace gpu {
namespace gles2 {

// Specialization of GLImage that allows us to support (stream) textures
// that supply a texture matrix.
class GPU_EXPORT GLStreamTextureImage : public gl::GLImage {
 public:
  GLStreamTextureImage() {}

  // Get the matrix.
  // Copy the texture matrix for this image into |matrix|.
  virtual void GetTextureMatrix(float matrix[16]) = 0;

 protected:
  ~GLStreamTextureImage() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(GLStreamTextureImage);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GL_STREAM_TEXTURE_IMAGE_H_

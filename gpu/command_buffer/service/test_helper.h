// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_TEST_HELPER_H_
#define GPU_COMMAND_BUFFER_SERVICE_TEST_HELPER_H_

#include "gpu/command_buffer/common/gl_mock.h"
#include "gpu/GLES2/gles2_command_buffer.h"

namespace gpu {
namespace gles2 {

struct DisallowedExtensions;

class TestHelper {
 public:
  static const GLuint kServiceBlackTexture2dId = 701;
  static const GLuint kServiceBlackTextureCubemapId = 702;
  static const GLuint kServiceDefaultTexture2dId = 703;
  static const GLuint kServiceDefaultTextureCubemapId = 704;

  static const GLint kMaxRenderbufferSize = 1024;
  static const GLint kMaxTextureSize = 2048;
  static const GLint kMaxCubeMapTextureSize = 256;
  static const GLint kNumVertexAttribs = 16;
  static const GLint kNumTextureUnits = 8;
  static const GLint kMaxTextureImageUnits = 8;
  static const GLint kMaxVertexTextureImageUnits = 2;
  static const GLint kMaxFragmentUniformVectors = 16;
  static const GLint kMaxFragmentUniformComponents =
      kMaxFragmentUniformVectors * 4;
  static const GLint kMaxVaryingVectors = 8;
  static const GLint kMaxVaryingFloats = kMaxVaryingVectors * 4;
  static const GLint kMaxVertexUniformVectors = 128;
  static const GLint kMaxVertexUniformComponents = kMaxVertexUniformVectors * 4;

  static void SetupContextGroupInitExpectations(
      ::gfx::MockGLInterface* gl,
      const DisallowedExtensions& disallowed_extensions,
      const char* extensions);
  static void SetupFeatureInfoInitExpectations(
      ::gfx::MockGLInterface* gl, const char* extensions);
  static void SetupTextureManagerInitExpectations(::gfx::MockGLInterface* gl);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_TEST_HELPER_H_


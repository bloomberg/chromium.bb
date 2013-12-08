// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_TEST_GLES2_INTERFACE_H_
#define CC_TEST_TEST_GLES2_INTERFACE_H_

#include "gpu/command_buffer/client/gles2_interface_stub.h"

namespace cc {
class TestWebGraphicsContext3D;

class TestGLES2Interface : public gpu::gles2::GLES2InterfaceStub {
 public:
  explicit TestGLES2Interface(TestWebGraphicsContext3D* test_context);
  virtual ~TestGLES2Interface();

  virtual GLuint CreateShader(GLenum type) OVERRIDE;
  virtual GLuint CreateProgram() OVERRIDE;
  virtual void GetShaderiv(GLuint shader, GLenum pname, GLint* params) OVERRIDE;
  virtual void GetProgramiv(GLuint program,
                            GLenum pname,
                            GLint* params) OVERRIDE;
  virtual void GetShaderPrecisionFormat(GLenum shadertype,
                                        GLenum precisiontype,
                                        GLint* range,
                                        GLint* precision) OVERRIDE;
  virtual void UseProgram(GLuint program) OVERRIDE;

 private:
  TestWebGraphicsContext3D* test_context_;
};

}  // namespace cc

#endif  // CC_TEST_TEST_GLES2_INTERFACE_H_

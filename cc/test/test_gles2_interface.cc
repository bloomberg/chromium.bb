// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_gles2_interface.h"

#include "base/logging.h"
#include "cc/test/test_web_graphics_context_3d.h"

namespace cc {

TestGLES2Interface::TestGLES2Interface(TestWebGraphicsContext3D* test_context)
    : test_context_(test_context) {
  DCHECK(test_context_);
}

TestGLES2Interface::~TestGLES2Interface() {}

GLuint TestGLES2Interface::CreateShader(GLenum type) {
  return test_context_->createShader(type);
}

GLuint TestGLES2Interface::CreateProgram() {
  return test_context_->createProgram();
}

void TestGLES2Interface::GetShaderiv(GLuint shader,
                                     GLenum pname,
                                     GLint* params) {
  test_context_->getShaderiv(shader, pname, params);
}

void TestGLES2Interface::GetProgramiv(GLuint program,
                                      GLenum pname,
                                      GLint* params) {
  test_context_->getProgramiv(program, pname, params);
}

void TestGLES2Interface::GetShaderPrecisionFormat(GLenum shadertype,
                                                  GLenum precisiontype,
                                                  GLint* range,
                                                  GLint* precision) {
  test_context_->getShaderPrecisionFormat(
      shadertype, precisiontype, range, precision);
}

void TestGLES2Interface::UseProgram(GLuint program) {
  test_context_->useProgram(program);
}

}  // namespace cc

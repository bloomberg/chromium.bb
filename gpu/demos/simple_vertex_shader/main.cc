// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a simple example that draws a single triangle with
// a minimal vertex/fragment shader.  The purpose of this
// example is to demonstrate the basic concepts of
// OpenGL ES 2.0 rendering.

#include "gpu/demos/app_framework/application.h"
#include "third_party/gles2_book/Chapter_8/Simple_VertexShader/Simple_VertexShader.h"

namespace gpu_demos {
class SimpleVertexShader : public Application {
 public:
  SimpleVertexShader();
  ~SimpleVertexShader();

  bool Init();

 protected:
  virtual void Draw(float elapsed_sec);

 private:
  ESContext context_;
  SVSUserData user_data_;

  DISALLOW_COPY_AND_ASSIGN(SimpleVertexShader);
};

SimpleVertexShader::SimpleVertexShader() {
  esInitContext(&context_);

  memset(&user_data_, 0, sizeof(SVSUserData));
  context_.userData = &user_data_;
}

SimpleVertexShader::~SimpleVertexShader() {
  svsShutDown(&context_);
}

bool SimpleVertexShader::Init() {
  if (!Application::InitRenderContext()) return false;

  context_.width = width();
  context_.height = height();
  if (!svsInit(&context_)) return false;

  return true;
}

void SimpleVertexShader::Draw(float elapsed_sec) {
  svsUpdate(&context_, elapsed_sec);

  svsDraw(&context_);
}
}  // namespace gpu_demos

int main(int argc, char *argv[]) {
  gpu_demos::SimpleVertexShader app;
  if (!app.Init()) {
    printf("Could not init.\n");
    return EXIT_FAILURE;
  }

  app.MainLoop();
  return EXIT_SUCCESS;
}

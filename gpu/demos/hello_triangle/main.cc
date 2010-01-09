// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a simple example that draws a single triangle with
// a minimal vertex/fragment shader.  The purpose of this
// example is to demonstrate the basic concepts of
// OpenGL ES 2.0 rendering.

#include "gpu/demos/app_framework/application.h"
#include "third_party/gles2_book/Chapter_2/Hello_Triangle/Hello_Triangle.h"

namespace gpu_demos {
class HelloTriangle : public Application {
 public:
  HelloTriangle();
  ~HelloTriangle();

  bool Init();

 protected:
  virtual void Draw();

 private:
  ESContext context_;
  HTUserData user_data_;

  DISALLOW_COPY_AND_ASSIGN(HelloTriangle);
};

HelloTriangle::HelloTriangle() {
  esInitContext(&context_);

  memset(&user_data_, 0, sizeof(HTUserData));
  context_.userData = &user_data_;
}

HelloTriangle::~HelloTriangle() {
  htShutDown(&context_);
}

bool HelloTriangle::Init() {
  if (!Application::InitRenderContext()) return false;

  context_.width = width();
  context_.height = height();
  if (!htInit(&context_)) return false;

  return true;
}

void HelloTriangle::Draw() {
  htDraw(&context_);
}
}  // namespace gpu_demos

int main(int argc, char *argv[]) {
  gpu_demos::HelloTriangle app;
  if (!app.Init()) {
    printf("Could not init.\n");
    return EXIT_FAILURE;
  }

  app.MainLoop();
  return EXIT_SUCCESS;
}

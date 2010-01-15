// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a simple example that draws a quad with a 2D
// texture image. The purpose of this example is to demonstrate
// the basics of 2D texturing

#include "gpu/demos/app_framework/application.h"
#include "third_party/gles2_book/Chapter_9/Simple_Texture2D/Simple_Texture2D.h"

namespace gpu_demos {
class SimpleTexture2D : public Application {
 public:
  SimpleTexture2D();
  ~SimpleTexture2D();

  bool Init();

 protected:
  virtual void Draw(float elapsed_sec);

 private:
  ESContext context_;
  STUserData user_data_;

  DISALLOW_COPY_AND_ASSIGN(SimpleTexture2D);
};

SimpleTexture2D::SimpleTexture2D() {
  esInitContext(&context_);

  memset(&user_data_, 0, sizeof(STUserData));
  context_.userData = &user_data_;
}

SimpleTexture2D::~SimpleTexture2D() {
  stShutDown(&context_);
}

bool SimpleTexture2D::Init() {
  if (!Application::InitRenderContext()) return false;

  context_.width = width();
  context_.height = height();
  if (!stInit(&context_)) return false;

  return true;
}

void SimpleTexture2D::Draw(float /*elapsed_sec*/) {
  stDraw(&context_);
}
}  // namespace gpu_demos

int main(int argc, char *argv[]) {
  gpu_demos::SimpleTexture2D app;
  if (!app.Init()) {
    printf("Could not init.\n");
    return EXIT_FAILURE;
  }

  app.MainLoop();
  return EXIT_SUCCESS;
}

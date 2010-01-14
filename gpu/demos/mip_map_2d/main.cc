// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a simple example that demonstrates generating a mipmap chain
// and rendering with it.

#include "gpu/demos/app_framework/application.h"
#include "third_party/gles2_book/Chapter_9/MipMap2D/MipMap2D.h"

namespace gpu_demos {
class MipMap2D : public Application {
 public:
  MipMap2D();
  ~MipMap2D();

  bool Init();

 protected:
  virtual void Draw(float);

 private:
  ESContext context_;
  MMUserData user_data_;

  DISALLOW_COPY_AND_ASSIGN(MipMap2D);
};

MipMap2D::MipMap2D() {
  esInitContext(&context_);

  memset(&user_data_, 0, sizeof(MMUserData));
  context_.userData = &user_data_;
}

MipMap2D::~MipMap2D() {
  mmShutDown(&context_);
}

bool MipMap2D::Init() {
  if (!Application::InitRenderContext()) return false;

  context_.width = width();
  context_.height = height();
  if (!mmInit(&context_)) return false;

  return true;
}

void MipMap2D::Draw(float) {
  mmDraw(&context_);
}
}  // namespace gpu_demos

int main(int argc, char *argv[]) {
  gpu_demos::MipMap2D app;
  if (!app.Init()) {
    printf("Could not init.\n");
    return EXIT_FAILURE;
  }

  app.MainLoop();
  return EXIT_SUCCESS;
}

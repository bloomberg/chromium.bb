// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a simple example that draws a sphere with a cubemap image applied.

#include "gpu/demos/app_framework/application.h"
#include "third_party/gles2_book/Chapter_9/Simple_TextureCubemap/Simple_TextureCubemap.h"

namespace gpu_demos {
class SimpleTextureCubemap : public Application {
 public:
  SimpleTextureCubemap();
  ~SimpleTextureCubemap();

  bool Init();

 protected:
  virtual void Draw(float elapsed_sec);

 private:
  ESContext context_;
  STCUserData user_data_;

  DISALLOW_COPY_AND_ASSIGN(SimpleTextureCubemap);
};

SimpleTextureCubemap::SimpleTextureCubemap() {
  esInitContext(&context_);

  memset(&user_data_, 0, sizeof(STCUserData));
  context_.userData = &user_data_;
}

SimpleTextureCubemap::~SimpleTextureCubemap() {
  stcShutDown(&context_);
}

bool SimpleTextureCubemap::Init() {
  if (!Application::InitRenderContext()) return false;

  context_.width = width();
  context_.height = height();
  if (!stcInit(&context_)) return false;

  return true;
}

void SimpleTextureCubemap::Draw(float /*elapsed_sec*/) {
  stcDraw(&context_);
}
}  // namespace gpu_demos

int main(int argc, char *argv[]) {
  gpu_demos::SimpleTextureCubemap app;
  if (!app.Init()) {
    printf("Could not init.\n");
    return EXIT_FAILURE;
  }

  app.MainLoop();
  return EXIT_SUCCESS;
}

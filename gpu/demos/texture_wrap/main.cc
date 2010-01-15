// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is an example that demonstrates the three texture
// wrap modes available on 2D textures.

#include "gpu/demos/app_framework/application.h"
#include "third_party/gles2_book/Chapter_9/TextureWrap/TextureWrap.h"

namespace gpu_demos {
class TextureWrap : public Application {
 public:
  TextureWrap();
  ~TextureWrap();

  bool Init();

 protected:
  virtual void Draw(float elapsed_sec);

 private:
  ESContext context_;
  TWUserData user_data_;

  DISALLOW_COPY_AND_ASSIGN(TextureWrap);
};

TextureWrap::TextureWrap() {
  esInitContext(&context_);

  memset(&user_data_, 0, sizeof(TWUserData));
  context_.userData = &user_data_;
}

TextureWrap::~TextureWrap() {
  twShutDown(&context_);
}

bool TextureWrap::Init() {
  if (!Application::InitRenderContext()) return false;

  context_.width = width();
  context_.height = height();
  if (!twInit(&context_)) return false;

  return true;
}

void TextureWrap::Draw(float /*elapsed_sec*/) {
  twDraw(&context_);
}
}  // namespace gpu_demos

int main(int argc, char *argv[]) {
  gpu_demos::TextureWrap app;
  if (!app.Init()) {
    printf("Could not init.\n");
    return EXIT_FAILURE;
  }

  app.MainLoop();
  return EXIT_SUCCESS;
}

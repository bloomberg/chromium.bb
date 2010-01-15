// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This example shows various stencil buffer operations.

#include "gpu/demos/app_framework/application.h"
#include "third_party/gles2_book/Chapter_11/Stencil_Test/Stencil_Test.h"

namespace gpu_demos {
class StencilTest : public Application {
 public:
  StencilTest();
  ~StencilTest();

  bool Init();

 protected:
  virtual void Draw(float elapsed_sec);

 private:
  ESContext context_;
  STUserData user_data_;

  DISALLOW_COPY_AND_ASSIGN(StencilTest);
};

StencilTest::StencilTest() {
  esInitContext(&context_);

  memset(&user_data_, 0, sizeof(STUserData));
  context_.userData = &user_data_;
}

StencilTest::~StencilTest() {
  stShutDown(&context_);
}

bool StencilTest::Init() {
  if (!Application::InitRenderContext()) return false;

  context_.width = width();
  context_.height = height();
  if (!stInit(&context_)) return false;

  return true;
}

void StencilTest::Draw(float /*elapsed_sec*/) {
  stDraw(&context_);
}
}  // namespace gpu_demos

int main(int argc, char *argv[]) {
  gpu_demos::StencilTest app;
  if (!app.Init()) {
    printf("Could not init.\n");
    return EXIT_FAILURE;
  }

  app.MainLoop();
  return EXIT_SUCCESS;
}

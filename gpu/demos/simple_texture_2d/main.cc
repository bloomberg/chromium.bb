// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a simple example that draws a quad with a 2D
// texture image. The purpose of this example is to demonstrate
// the basics of 2D texturing

#include "gpu/demos/gles2_book/example.h"
#include "third_party/gles2_book/Chapter_9/Simple_Texture2D/Simple_Texture2D.h"

namespace gpu {
namespace demos {
namespace gles2_book {
typedef Example<STUserData,
                stInit,
                NoOpUpdateFunc,
                stDraw,
                stShutDown> SimpleTexture2D;
}  // namespace gles2_book
}  // namespace demos
}  // namespace gpu

int main(int argc, char *argv[]) {
  gpu::demos::gles2_book::SimpleTexture2D demo;
  CHECK(demo.Init());

  demo.MainLoop();
  return EXIT_SUCCESS;
}

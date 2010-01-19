// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a simple example that draws a sphere with a cubemap image applied.

#include "gpu/demos/gles2_book/example.h"
#include "third_party/gles2_book/Chapter_9/Simple_TextureCubemap/Simple_TextureCubemap.h"

namespace gpu {
namespace demos {
namespace gles2_book {
typedef Example<STCUserData,
                stcInit,
                NoOpUpdateFunc,
                stcDraw,
                stcShutDown> SimpleTextureCubemap;
}  // namespace gles2_book
}  // namespace demos
}  // namespace gpu

int main(int argc, char *argv[]) {
  gpu::demos::gles2_book::SimpleTextureCubemap demo;
  CHECK(demo.Init());

  demo.MainLoop();
  return EXIT_SUCCESS;
}

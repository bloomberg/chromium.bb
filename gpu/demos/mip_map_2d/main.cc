// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a simple example that demonstrates generating a mipmap chain
// and rendering with it.

#include "gpu/demos/gles2_book/example.h"
#include "third_party/gles2_book/Chapter_9/MipMap2D/MipMap2D.h"

namespace gpu {
namespace demos {
namespace gles2_book {
typedef Example<MMUserData,
                mmInit,
                NoOpUpdateFunc,
                mmDraw,
                mmShutDown> MipMap2D;
}  // namespace gles2_book
}  // namespace demos
}  // namespace gpu

int main(int argc, char *argv[]) {
  gpu::demos::gles2_book::MipMap2D demo;
  CHECK(demo.Init());

  demo.MainLoop();
  return EXIT_SUCCESS;
}

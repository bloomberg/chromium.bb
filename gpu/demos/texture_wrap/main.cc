// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is an example that demonstrates the three texture
// wrap modes available on 2D textures.

#include "gpu/demos/gles2_book/example.h"
#include "third_party/gles2_book/Chapter_9/TextureWrap/TextureWrap.h"

namespace gpu {
namespace demos {
namespace gles2_book {
typedef Example<TWUserData,
                twInit,
                NoOpUpdateFunc,
                twDraw,
                twShutDown> TextureWrap;
}  // namespace gles2_book
}  // namespace demos
}  // namespace gpu

int main(int argc, char *argv[]) {
  gpu::demos::gles2_book::TextureWrap demo;
  CHECK(demo.Init());

  demo.MainLoop();
  return EXIT_SUCCESS;
}

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This example shows various stencil buffer operations.

#include "gpu/demos/gles2_book/example.h"
#include "third_party/gles2_book/Chapter_11/Stencil_Test/Stencil_Test.h"

namespace gpu {
namespace demos {
namespace gles2_book {
typedef Example<STUserData,
                stInit,
                NoOpUpdateFunc,
                stDraw,
                stShutDown> StencilTest;
}  // namespace gles2_book
}  // namespace demos
}  // namespace gpu

int main(int argc, char *argv[]) {
  gpu::demos::gles2_book::StencilTest demo;
  CHECK(demo.Init());

  demo.MainLoop();
  return EXIT_SUCCESS;
}

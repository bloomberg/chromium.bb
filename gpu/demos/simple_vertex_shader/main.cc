// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a simple example that draws a single triangle with
// a minimal vertex/fragment shader.  The purpose of this
// example is to demonstrate the basic concepts of
// OpenGL ES 2.0 rendering.

#include "gpu/demos/gles2_book/example.h"
#include "third_party/gles2_book/Chapter_8/Simple_VertexShader/Simple_VertexShader.h"

namespace gpu {
namespace demos {
namespace gles2_book {
typedef Example<SVSUserData,
                svsInit,
                svsUpdate,
                svsDraw,
                svsShutDown> SimpleVertexShader;
}  // namespace gles2_book
}  // namespace demos
}  // namespace gpu

int main(int argc, char *argv[]) {
  gpu::demos::gles2_book::SimpleVertexShader demo;
  CHECK(demo.Init());

  demo.MainLoop();
  return EXIT_SUCCESS;
}

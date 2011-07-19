// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a simple example that draws a single triangle with
// a minimal vertex/fragment shader.  The purpose of this
// example is to demonstrate the basic concepts of
// OpenGL ES 2.0 rendering.

#include "gpu/demos/framework/demo_factory.h"
#include "gpu/demos/gles2_book/example.h"
#include "third_party/gles2_book/Chapter_2/Hello_Triangle/Hello_Triangle.h"

namespace gpu {
namespace demos {

namespace gles2_book {
class HelloTriangle : public Example<HTUserData> {
 public:
  HelloTriangle() {
    RegisterCallbacks(htInit, NULL, htDraw, htShutDown);
  }

  const wchar_t* Title() const {
    return L"Hello Triangle";
  }
};
}  // namespace gles2_book

Demo* CreateDemo() {
  return new gles2_book::HelloTriangle();
}

}  // namespace demos
}  // namespace gpu

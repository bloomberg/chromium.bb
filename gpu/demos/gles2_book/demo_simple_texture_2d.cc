// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a simple example that draws a quad with a 2D
// texture image. The purpose of this example is to demonstrate
// the basics of 2D texturing.

#include "gpu/demos/framework/demo_factory.h"
#include "gpu/demos/gles2_book/example.h"
#include "third_party/gles2_book/Chapter_9/Simple_Texture2D/Simple_Texture2D.h"

namespace gpu {
namespace demos {

namespace gles2_book {
class SimpleTexture2D : public Example<STUserData> {
 public:
  SimpleTexture2D() {
    RegisterCallbacks(stInit, NULL, stDraw, stShutDown);
  }

  const wchar_t* Title() const {
    return L"Simple Texture 2D";
  }
};
}  // namespace gles2_book

Demo* CreateDemo() {
  return new gles2_book::SimpleTexture2D();
}

}  // namespace demos
}  // namespace gpu

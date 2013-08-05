// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a simple example that draws a sphere with a cubemap image applied.

#include "gpu/demos/framework/demo_factory.h"
#include "gpu/demos/gles2_book/example.h"
#include "third_party/gles2_book/Chapter_9/Simple_TextureCubemap/Simple_TextureCubemap.h"

namespace gpu {
namespace demos {

namespace gles2_book {
class SimpleTextureCubemap : public Example<STCUserData> {
 public:
  SimpleTextureCubemap() {
    RegisterCallbacks(stcInit, NULL, stcDraw, stcShutDown);
  }

  const wchar_t* Title() const {
    return L"Simple Texture Cubemap";
  }
};
}  // namespace gles2_book

Demo* CreateDemo() {
  return new gles2_book::SimpleTextureCubemap();
}

}  // namespace demos
}  // namespace gpu

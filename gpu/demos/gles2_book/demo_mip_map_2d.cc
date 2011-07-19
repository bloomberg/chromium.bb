// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a simple example that demonstrates generating a mipmap chain
// and rendering with it.

#include "gpu/demos/framework/demo_factory.h"
#include "gpu/demos/gles2_book/example.h"
#include "third_party/gles2_book/Chapter_9/MipMap2D/MipMap2D.h"

namespace gpu {
namespace demos {

namespace gles2_book {
class MipMap2D : public Example<MMUserData> {
 public:
  MipMap2D() {
    RegisterCallbacks(mmInit, NULL, mmDraw, mmShutDown);
  }

  const wchar_t* Title() const {
    return L"Mip-Map 2D";
  }
};
}  // namespace gles2_book

Demo* CreateDemo() {
  return new gles2_book::MipMap2D();
}

}  // namespace demos
}  // namespace gpu

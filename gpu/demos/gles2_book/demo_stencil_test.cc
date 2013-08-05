// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This example shows various stencil buffer operations.

#include "gpu/demos/framework/demo_factory.h"
#include "gpu/demos/gles2_book/example.h"
#include "third_party/gles2_book/Chapter_11/Stencil_Test/Stencil_Test.h"

namespace gpu {
namespace demos {

namespace gles2_book {
class StencilTest : public Example<STUserData> {
 public:
  StencilTest() {
    RegisterCallbacks(stInit, NULL, stDraw, stShutDown);
  }

  const wchar_t* Title() const {
    return L"Stencil Test";
  }
};
}  // namespace gles2_book

Demo* CreateDemo() {
  return new gles2_book::StencilTest();
}

}  // namespace demos
}  // namespace gpu

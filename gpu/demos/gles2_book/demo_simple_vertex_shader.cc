// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a simple example that draws a rotating cube in perspective
// using a vertex shader to transform the object.

#include "gpu/demos/framework/demo_factory.h"
#include "gpu/demos/gles2_book/example.h"
#include "third_party/gles2_book/Chapter_8/Simple_VertexShader/Simple_VertexShader.h"

namespace gpu {
namespace demos {

namespace gles2_book {
class SimpleVertexShader : public Example<SVSUserData> {
 public:
  SimpleVertexShader() {
    RegisterCallbacks(svsInit, svsUpdate, svsDraw, svsShutDown);
  }

  const wchar_t* Title() const {
    return L"Simple Vertex Shader";
  }

  virtual bool IsAnimated() {
    return true;
  }
};
}  // namespace gles2_book

Demo* CreateDemo() {
  return new gles2_book::SimpleVertexShader();
}

}  // namespace demos
}  // namespace gpu

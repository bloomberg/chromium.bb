// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is an example that demonstrates the three texture
// wrap modes available on 2D textures.

#include "gpu/demos/framework/demo_factory.h"
#include "gpu/demos/gles2_book/example.h"
#include "third_party/gles2_book/Chapter_9/TextureWrap/TextureWrap.h"

namespace gpu {
namespace demos {

namespace gles2_book {
class TextureWrap : public Example<TWUserData> {
 public:
  TextureWrap() {
    RegisterCallbacks(twInit, NULL, twDraw, twShutDown);
  }

  const wchar_t* Title() const {
    return L"Texture Wrap";
  }
};
}  // namespace gles2_book

Demo* CreateDemo() {
  return new gles2_book::TextureWrap();
}

}  // namespace demos
}  // namespace gpu

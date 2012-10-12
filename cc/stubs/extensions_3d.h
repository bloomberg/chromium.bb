// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_STUBS_EXTENSIONS3D_H_
#define CC_STUBS_EXTENSIONS3D_H_

#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"

namespace cc {

class Extensions3D {
public:
    enum {
        TEXTURE_RECTANGLE_ARB = GL_TEXTURE_RECTANGLE_ARB,
        BGRA_EXT = GL_BGRA_EXT,
        RGBA8_OES = GL_RGBA8_OES,
    };
};

}


#endif  // CC_STUBS_EXTENSIONS3D_H_

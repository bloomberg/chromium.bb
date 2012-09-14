// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_STUBS_GRAPHICSCONTEXT3D_H_
#define CC_STUBS_GRAPHICSCONTEXT3D_H_

#include "GraphicsTypes3D.h"
#include "IntSize.h"
#include "third_party/khronos/GLES2/gl2.h"

#ifdef NO_ERROR
#undef NO_ERROR
#endif

namespace cc {

class GraphicsContext3D {
public:
    enum SourceDataFormat { SourceFormatRGBA8, SourceFormatBGRA8 };
    static bool computeFormatAndTypeParameters(unsigned, unsigned, unsigned* componentsPerPixel, unsigned* bytesPerComponent);

    enum {
        ARRAY_BUFFER = GL_ARRAY_BUFFER,
        BLEND = GL_BLEND,
        CLAMP_TO_EDGE = GL_CLAMP_TO_EDGE,
        COLOR_ATTACHMENT0 = GL_COLOR_ATTACHMENT0,
        COLOR_BUFFER_BIT = GL_COLOR_BUFFER_BIT,
        COMPILE_STATUS = GL_COMPILE_STATUS,
        CULL_FACE = GL_CULL_FACE,
        DEPTH_TEST = GL_DEPTH_TEST,
        ELEMENT_ARRAY_BUFFER = GL_ELEMENT_ARRAY_BUFFER,
        EXTENSIONS = GL_EXTENSIONS,
        FLOAT = GL_FLOAT,
        FRAGMENT_SHADER = GL_FRAGMENT_SHADER,
        FRAMEBUFFER_COMPLETE = GL_FRAMEBUFFER_COMPLETE,
        FRAMEBUFFER = GL_FRAMEBUFFER,
        INVALID_ENUM = GL_INVALID_ENUM,
        INVALID_VALUE = GL_INVALID_VALUE,
        LINEAR = GL_LINEAR,
        LINE_LOOP = GL_LINE_LOOP ,
        LINK_STATUS = GL_LINK_STATUS,
        LUMINANCE = GL_LUMINANCE,
        MAX_TEXTURE_SIZE = GL_MAX_TEXTURE_SIZE,
        NEAREST = GL_NEAREST,
        NO_ERROR = GL_NO_ERROR,
        ONE = GL_ONE,
        ONE_MINUS_SRC_ALPHA = GL_ONE_MINUS_SRC_ALPHA,
        RGBA = GL_RGBA,
        RGB = GL_RGB,
        SCISSOR_TEST = GL_SCISSOR_TEST,
        SRC_ALPHA = GL_SRC_ALPHA,
        STATIC_DRAW = GL_STATIC_DRAW,
        TEXTURE0 = GL_TEXTURE0,
        TEXTURE1 = GL_TEXTURE1,
        TEXTURE_2D = GL_TEXTURE_2D,
        TEXTURE2 = GL_TEXTURE2,
        TEXTURE3 = GL_TEXTURE3,
        TEXTURE_MAG_FILTER = GL_TEXTURE_MAG_FILTER,
        TEXTURE_MIN_FILTER = GL_TEXTURE_MIN_FILTER,
        TEXTURE_WRAP_S = GL_TEXTURE_WRAP_S,
        TEXTURE_WRAP_T = GL_TEXTURE_WRAP_T,
        TRIANGLES = GL_TRIANGLES,
        TRIANGLE_FAN = GL_TRIANGLE_FAN,
        UNSIGNED_BYTE = GL_UNSIGNED_BYTE,
        UNSIGNED_SHORT = GL_UNSIGNED_SHORT,
        VERTEX_SHADER = GL_VERTEX_SHADER,
        ZERO = GL_ZERO,
    };
};

}

#endif  // CC_STUBS_GRAPHICSCONTEXT3D_H_

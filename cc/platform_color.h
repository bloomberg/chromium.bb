// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PLATFORM_COLOR_H_
#define CC_PLATFORM_COLOR_H_

#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkTypes.h"

namespace cc {

class PlatformColor {
public:
    enum SourceDataFormat { SourceFormatRGBA8, SourceFormatBGRA8 };

    static SourceDataFormat format()
    {
        return SK_B32_SHIFT ? SourceFormatRGBA8 : SourceFormatBGRA8;
    }

    // Returns the most efficient texture format for this platform.
    static GLenum bestTextureFormat(WebKit::WebGraphicsContext3D* context, bool supportsBGRA8888)
    {
        GLenum textureFormat = GL_RGBA;
        switch (format()) {
        case SourceFormatRGBA8:
            break;
        case SourceFormatBGRA8:
            if (supportsBGRA8888)
                textureFormat = GL_BGRA_EXT;
            break;
        default:
            NOTREACHED();
            break;
        }
        return textureFormat;
    }

    // Return true if the given texture format has the same component order
    // as the color on this platform.
    static bool sameComponentOrder(GLenum textureFormat)
    {
        switch (format()) {
        case SourceFormatRGBA8:
            return textureFormat == GL_RGBA;
        case SourceFormatBGRA8:
            return textureFormat == GL_BGRA_EXT;
        default:
            NOTREACHED();
            return false;
        }
    }
};

} // namespace cc

#endif  // CC_PLATFORM_COLOR_H_

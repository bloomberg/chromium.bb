// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PlatformColor_h
#define PlatformColor_h

#include "Extensions3D.h"
#include "GraphicsContext3D.h"
#include "SkTypes.h" 
#include <public/WebGraphicsContext3D.h>

namespace cc {

class PlatformColor {
public:
    static GraphicsContext3D::SourceDataFormat format()
    {
        return SK_B32_SHIFT ? GraphicsContext3D::SourceFormatRGBA8 : GraphicsContext3D::SourceFormatBGRA8;
    }

    // Returns the most efficient texture format for this platform.
    static GC3Denum bestTextureFormat(WebKit::WebGraphicsContext3D* context, bool supportsBGRA8888)
    {
        GC3Denum textureFormat = GraphicsContext3D::RGBA;
        switch (format()) {
        case GraphicsContext3D::SourceFormatRGBA8:
            break;
        case GraphicsContext3D::SourceFormatBGRA8:
            if (supportsBGRA8888)
                textureFormat = Extensions3D::BGRA_EXT;
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
        return textureFormat;
    }

    // Return true if the given texture format has the same component order
    // as the color on this platform.
    static bool sameComponentOrder(GC3Denum textureFormat)
    {
        switch (format()) {
        case GraphicsContext3D::SourceFormatRGBA8:
            return textureFormat == GraphicsContext3D::RGBA;
        case GraphicsContext3D::SourceFormatBGRA8:
            return textureFormat == Extensions3D::BGRA_EXT;
        default:
            ASSERT_NOT_REACHED();
            return false;
        }
    }
};

} // namespace cc

#endif

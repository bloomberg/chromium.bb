// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_STUBS_FLOATPOINT3D_H_
#define CC_STUBS_FLOATPOINT3D_H_

#include "FloatPoint.h"

#if INSIDE_WEBKIT_BUILD
#include "Source/WebCore/platform/graphics/FloatPoint3D.h"
#else
#include "third_party/WebKit/Source/WebCore/platform/graphics/FloatPoint3D.h"
#endif

namespace cc {

class FloatPoint3D : public WebCore::FloatPoint3D {
public:
    FloatPoint3D() { }

    FloatPoint3D(float x, float y, float z)
        : WebCore::FloatPoint3D(x, y, z)
    {
    }

    FloatPoint3D(FloatPoint point)
        : WebCore::FloatPoint3D(point.x(), point.y(), 0)
    {
    }

    FloatPoint3D(const FloatPoint3D& point)
        : WebCore::FloatPoint3D(point)
    {
    }

    FloatPoint3D(WebCore::FloatPoint3D point)
        : WebCore::FloatPoint3D(point)
    {
    }

    FloatPoint3D(WebCore::FloatPoint point)
        : WebCore::FloatPoint3D(point)
    {
    }
};

}

#endif

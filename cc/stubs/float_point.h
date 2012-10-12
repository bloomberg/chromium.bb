// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_STUBS_FLOATPOINT_H_
#define CC_STUBS_FLOATPOINT_H_

#include "FloatSize.h"
#include "IntPoint.h"
#if INSIDE_WEBKIT_BUILD
#include "Source/WebCore/platform/graphics/FloatPoint.h"
#else
#include "third_party/WebKit/Source/WebCore/platform/graphics/FloatPoint.h"
#endif

namespace cc {

class FloatPoint : public WebCore::FloatPoint {
public:
    FloatPoint() { }

    FloatPoint(float width, float height)
        : WebCore::FloatPoint(width, height)
    {
    }

    FloatPoint(FloatSize size)
        : WebCore::FloatPoint(size)
    {
    }

    FloatPoint(IntPoint point)
        : WebCore::FloatPoint(point)
    {
    }

    FloatPoint(WebCore::IntPoint point)
        : WebCore::FloatPoint(point)
    {
    }

    FloatPoint(WebCore::FloatPoint point)
        : WebCore::FloatPoint(point.x(), point.y())
    {

    }
};

}

#endif

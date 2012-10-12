// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_STUBS_INTPOINT_H_
#define CC_STUBS_INTPOINT_H_

#include "IntSize.h"
#if INSIDE_WEBKIT_BUILD
#include "Source/WebCore/platform/graphics/IntPoint.h"
#else
#include "third_party/WebKit/Source/WebCore/platform/graphics/IntPoint.h"
#endif

namespace cc {

class IntPoint : public WebCore::IntPoint {
public:
    IntPoint() { }

    IntPoint(int width, int height)
        : WebCore::IntPoint(width, height)
    {
    }

    IntPoint(IntSize size)
        : WebCore::IntPoint(size.width(), size.height())
    {
    }

    IntPoint(WebCore::IntPoint point)
        : WebCore::IntPoint(point.x(), point.y())
    {

    }
};

}

#endif

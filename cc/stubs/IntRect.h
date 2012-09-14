// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_STUBS_INTRECT_H_
#define CC_STUBS_INTRECT_H_

#include "IntPoint.h"
#include "IntSize.h"
#if INSIDE_WEBKIT_BUILD
#include "Source/WebCore/platform/graphics/IntRect.h"
#else
#include "third_party/WebKit/Source/WebCore/platform/graphics/IntRect.h"
#endif

namespace cc {

class IntRect : public WebCore::IntRect {
public:
    IntRect() { }

    IntRect(const IntPoint& location, const IntSize& size)
        : WebCore::IntRect(location, size)
    {
    }

    IntRect(int x, int y, int width, int height)
        : WebCore::IntRect(x, y, width, height)
    {
    }

    IntRect(WebCore::IntRect rect)
        : WebCore::IntRect(rect.x(), rect.y(), rect.width(), rect.height())
    {

    }
};

}

#endif

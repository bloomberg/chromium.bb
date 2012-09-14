// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_STUBS_FLOATRECT_H_
#define CC_STUBS_FLOATRECT_H_

#include "FloatPoint.h"
#include "FloatSize.h"
#include "IntRect.h"
#if INSIDE_WEBKIT_BUILD
#include "Source/WebCore/platform/graphics/FloatRect.h"
#else
#include "third_party/WebKit/Source/WebCore/platform/graphics/FloatRect.h"
#endif

namespace cc {

class FloatRect : public WebCore::FloatRect {
public:
    FloatRect() { }

    FloatRect(const FloatPoint& location, const FloatSize& size)
        : WebCore::FloatRect(location, size)
    {
    }

    FloatRect(const IntPoint& location, const IntSize& size)
        : WebCore::FloatRect(location, size)
    {
    }

    FloatRect(float x, float y, float width, float height)
        : WebCore::FloatRect(x, y, width, height)
    {
    }

    FloatRect(const IntRect& rect)
        : WebCore::FloatRect(rect)
    {
    }

    FloatRect(const WebCore::IntRect& rect)
        : WebCore::FloatRect(rect)
    {
    }

    FloatRect(const WebCore::FloatRect& rect)
        :WebCore::FloatRect(rect)
    {
    }
};

}

#endif

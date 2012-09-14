// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_STUBS_FLOATQUAD_H_
#define CC_STUBS_FLOATQUAD_H_

#include "FloatPoint.h"
#include "FloatRect.h"
#if INSIDE_WEBKIT_BUILD
#include "Source/WebCore/platform/graphics/FloatQuad.h"
#else
#include "third_party/WebKit/Source/WebCore/platform/graphics/FloatQuad.h"
#endif

namespace cc {

class FloatQuad : public WebCore::FloatQuad
{
public:
    FloatQuad() { }

    FloatQuad(const FloatPoint& p1, const FloatPoint& p2, const FloatPoint& p3, const FloatPoint& p4)
        : WebCore::FloatQuad(p1, p2, p3, p4)
    {
    }

    FloatQuad(const FloatRect& rect)
        : WebCore::FloatQuad(rect)
    {
    }

    FloatQuad(const IntRect& rect)
        : WebCore::FloatQuad(rect)
    {
    }

    FloatQuad(const WebCore::FloatRect& rect)
        : WebCore::FloatQuad(rect)
    {
    }

    FloatQuad(const WebCore::IntRect& rect)
        : WebCore::FloatQuad(rect)
    {
    }

    FloatQuad(const WebCore::FloatQuad& quad)
        : WebCore::FloatQuad(quad)
    {
    }
};

}

#endif

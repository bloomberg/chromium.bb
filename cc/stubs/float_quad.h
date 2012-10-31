// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_STUBS_FLOATQUAD_H_
#define CC_STUBS_FLOATQUAD_H_

#include "ui/gfx/rect.h"
#include "ui/gfx/rect_f.h"
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

    FloatQuad(const gfx::Rect& rect)
        : WebCore::FloatQuad(WebCore::IntRect(rect.x(), rect.y(), rect.width(), rect.height()))
    {
    }

    FloatQuad(const gfx::RectF& rect)
        : WebCore::FloatQuad(WebCore::FloatRect(rect.x(), rect.y(), rect.width(), rect.height()))
    {
    }

    FloatQuad(const gfx::PointF& p1, const gfx::PointF& p2, const gfx::PointF& p3, const gfx::PointF& p4)
        : WebCore::FloatQuad(cc::FloatPoint(p1), cc::FloatPoint(p2), cc::FloatPoint(p3), cc::FloatPoint(p4))
    {
    }

    cc::FloatPoint p1() const { return cc::FloatPoint(WebCore::FloatQuad::p1()); }
    cc::FloatPoint p2() const { return cc::FloatPoint(WebCore::FloatQuad::p2()); }
    cc::FloatPoint p3() const { return cc::FloatPoint(WebCore::FloatQuad::p3()); }
    cc::FloatPoint p4() const { return cc::FloatPoint(WebCore::FloatQuad::p4()); }

    void setP1(gfx::PointF p) { WebCore::FloatQuad::setP1(cc::FloatPoint(p)); }
    void setP2(gfx::PointF p) { WebCore::FloatQuad::setP2(cc::FloatPoint(p)); }
    void setP3(gfx::PointF p) { WebCore::FloatQuad::setP3(cc::FloatPoint(p)); }
    void setP4(gfx::PointF p) { WebCore::FloatQuad::setP4(cc::FloatPoint(p)); }

    gfx::RectF boundingBox() const { return cc::FloatRect(WebCore::FloatQuad::boundingBox()); }

    bool containsPoint(gfx::PointF p) { return WebCore::FloatQuad::containsPoint(cc::FloatPoint(p)); }
};

}

#endif

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_STUBS_REGION_H_
#define CC_STUBS_REGION_H_

#include "IntRect.h"
#if INSIDE_WEBKIT_BUILD
#include "Source/WebCore/platform/graphics/Region.h"
#else
#include "third_party/WebKit/Source/WebCore/platform/graphics/Region.h"
#endif
#include "ui/gfx/rect.h"

namespace cc {

class Region : public WebCore::Region {
public:
    Region() { }

    Region(const IntRect& rect)
        : WebCore::Region(rect)
    {
    }

    Region(const WebCore::IntRect& rect)
        : WebCore::Region(rect)
    {
    }

    Region(const WebCore::Region& region)
        : WebCore::Region(region)
    {
    }

    Region(const gfx::Rect& rect)
        : WebCore::Region(WebCore::IntRect(rect.x(), rect.y(), rect.width(), rect.height()))
    {
    }

    bool IsEmpty() const { return isEmpty(); }

    bool Contains(const gfx::Point& point) const { return contains(cc::IntPoint(point)); }
    bool Contains(const gfx::Rect& rect) const { return contains(cc::IntRect(rect)); }
    void Subtract(const gfx::Rect& rect) { subtract(cc::IntRect(rect)); }
    void Subtract(const Region& region) { subtract(region); }
    void Union(const gfx::Rect& rect) { unite(cc::IntRect(rect)); }
    void Union(const Region& region) { unite(region); }
    void Intersect(const gfx::Rect& rect) { intersect(cc::IntRect(rect)); }
    void Intersect(const Region& region) { intersect(region); }

    gfx::Rect bounds() const { return cc::IntRect(WebCore::Region::bounds()); }

private:
    bool isEmpty() const { return WebCore::Region::isEmpty(); }
    bool contains(const IntPoint& point) const { return WebCore::Region::contains(point); }
    bool contains(const IntRect& rect) const { return WebCore::Region::contains(rect); }
    void subtract(const IntRect& rect) { return WebCore::Region::subtract(rect); }
    void subtract(const Region& region) { return WebCore::Region::subtract(region); }
    void unite(const IntRect& rect) { return WebCore::Region::unite(rect); }
    void unite(const Region& region) { return WebCore::Region::unite(region); }
    void intersect(const IntRect& rect) { return WebCore::Region::intersect(rect); }
    void intersect(const Region& region) { return WebCore::Region::intersect(region); }
};

inline Region subtract(const Region& region, const gfx::Rect& rect) { return WebCore::intersect(region, cc::IntRect(rect)); }
inline Region intersect(const Region& region, const gfx::Rect& rect) { return WebCore::intersect(region, cc::IntRect(rect)); }

}

#endif  // CC_STUBS_REGION_H_

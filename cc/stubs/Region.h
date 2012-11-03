// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_STUBS_REGION_H_
#define CC_STUBS_REGION_H_

#if INSIDE_WEBKIT_BUILD
#include "Source/WebCore/platform/graphics/Region.h"
#else
#include "third_party/WebKit/Source/WebCore/platform/graphics/Region.h"
#endif
#include "base/logging.h"
#include "ui/gfx/rect.h"

namespace cc {

class Region : public WebCore::Region {
public:
    Region() { }

    Region(const WebCore::Region& region)
        : WebCore::Region(region)
    {
    }

    Region(const gfx::Rect& rect)
        : WebCore::Region(WebCore::IntRect(rect.x(), rect.y(), rect.width(), rect.height()))
    {
    }

    bool IsEmpty() const { return isEmpty(); }

    bool Contains(const gfx::Point& point) const { return contains(WebCore::IntPoint(point.x(), point.y())); }
    bool Contains(const gfx::Rect& rect) const { return contains(WebCore::IntRect(rect.x(), rect.y(), rect.width(), rect.height())); }
    void Subtract(const gfx::Rect& rect) { subtract(WebCore::IntRect(rect.x(), rect.y(), rect.width(), rect.height())); }
    void Subtract(const Region& region) { subtract(region); }
    void Union(const gfx::Rect& rect) { unite(WebCore::IntRect(rect.x(), rect.y(), rect.width(), rect.height())); }
    void Union(const Region& region) { unite(region); }
    void Intersect(const gfx::Rect& rect) { intersect(WebCore::IntRect(rect.x(), rect.y(), rect.width(), rect.height())); }
    void Intersect(const Region& region) { intersect(region); }

    gfx::Rect bounds() const
    {
        WebCore::IntRect bounds = WebCore::Region::bounds();
        return gfx::Rect(bounds.x(), bounds.y(), bounds.width(), bounds.height());
    }

    class Iterator {
    public:
        Iterator(const Region& region);
        ~Iterator();

        gfx::Rect rect() const {
            DCHECK(has_rect());
            if (!has_rect())
                return gfx::Rect();
            return gfx::Rect(m_rects[m_pos].x(), m_rects[m_pos].y(), m_rects[m_pos].width(), m_rects[m_pos].height());
        }

        void next() { ++m_pos; }
        bool has_rect() const { return m_pos < m_rects.size(); }

        // It is expensive to construct the iterator just to get this size. Only
        // do this for testing.
        size_t size() const { return m_rects.size(); }
    private:
        size_t m_pos;
        Vector<WebCore::IntRect> m_rects;
    };

private:
    bool isEmpty() const { return WebCore::Region::isEmpty(); }
    bool contains(const WebCore::IntPoint& point) const { return WebCore::Region::contains(point); }
    bool contains(const WebCore::IntRect& rect) const { return WebCore::Region::contains(rect); }
    void subtract(const WebCore::IntRect& rect) { return WebCore::Region::subtract(rect); }
    void subtract(const Region& region) { return WebCore::Region::subtract(region); }
    void unite(const WebCore::IntRect& rect) { return WebCore::Region::unite(rect); }
    void unite(const Region& region) { return WebCore::Region::unite(region); }
    void intersect(const WebCore::IntRect& rect) { return WebCore::Region::intersect(rect); }
    void intersect(const Region& region) { return WebCore::Region::intersect(region); }
    Vector<WebCore::IntRect> rects() const { return WebCore::Region::rects(); }
};

inline Region subtract(const Region& region, const gfx::Rect& rect) { return WebCore::intersect(region, WebCore::IntRect(rect.x(), rect.y(), rect.width(), rect.height())); }
inline Region intersect(const Region& region, const gfx::Rect& rect) { return WebCore::intersect(region, WebCore::IntRect(rect.x(), rect.y(), rect.width(), rect.height())); }

inline Region::Iterator::Iterator(const Region& region)
    : m_pos(0)
    , m_rects(region.rects())
{
}

inline Region::Iterator::~Iterator() { }

}

#endif  // CC_STUBS_REGION_H_

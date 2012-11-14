// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scrollbar_geometry_stub.h"

#include <cmath>

using WebKit::WebRect;
using WebKit::WebScrollbar;
using WebKit::WebScrollbarThemeGeometry;

namespace cc {

ScrollbarGeometryStub::ScrollbarGeometryStub(scoped_ptr<WebScrollbarThemeGeometry> geometry)
    : m_geometry(geometry.Pass())
{
}

ScrollbarGeometryStub::~ScrollbarGeometryStub()
{
}

WebScrollbarThemeGeometry* ScrollbarGeometryStub::clone() const
{
    return m_geometry->clone();
}

int ScrollbarGeometryStub::thumbPosition(WebScrollbar* scrollbar)
{
    return m_geometry->thumbPosition(scrollbar);
}

int ScrollbarGeometryStub::thumbLength(WebScrollbar* scrollbar)
{
    return std::max(0, m_geometry->thumbLength(scrollbar));
}

int ScrollbarGeometryStub::trackPosition(WebScrollbar* scrollbar)
{
    return m_geometry->trackPosition(scrollbar);
}

int ScrollbarGeometryStub::trackLength(WebScrollbar* scrollbar)
{
    return m_geometry->trackLength(scrollbar);
}

bool ScrollbarGeometryStub::hasButtons(WebScrollbar* scrollbar)
{
    return m_geometry->hasButtons(scrollbar);
}

bool ScrollbarGeometryStub::hasThumb(WebScrollbar* scrollbar)
{
    return m_geometry->hasThumb(scrollbar);
}

WebRect ScrollbarGeometryStub::trackRect(WebScrollbar* scrollbar)
{
    return m_geometry->trackRect(scrollbar);
}

WebRect ScrollbarGeometryStub::thumbRect(WebScrollbar* scrollbar)
{
    return m_geometry->thumbRect(scrollbar);
}

int ScrollbarGeometryStub::minimumThumbLength(WebScrollbar* scrollbar)
{
    return m_geometry->minimumThumbLength(scrollbar);
}

int ScrollbarGeometryStub::scrollbarThickness(WebScrollbar* scrollbar)
{
    return m_geometry->scrollbarThickness(scrollbar);
}

WebRect ScrollbarGeometryStub::backButtonStartRect(WebScrollbar* scrollbar)
{
    return m_geometry->backButtonStartRect(scrollbar);
}

WebRect ScrollbarGeometryStub::backButtonEndRect(WebScrollbar* scrollbar)
{
    return m_geometry->backButtonEndRect(scrollbar);
}

WebRect ScrollbarGeometryStub::forwardButtonStartRect(WebScrollbar* scrollbar)
{
    return m_geometry->forwardButtonStartRect(scrollbar);
}

WebRect ScrollbarGeometryStub::forwardButtonEndRect(WebScrollbar* scrollbar)
{
    return m_geometry->forwardButtonEndRect(scrollbar);
}

WebRect ScrollbarGeometryStub::constrainTrackRectToTrackPieces(WebScrollbar* scrollbar, const WebRect& rect)
{
    return m_geometry->constrainTrackRectToTrackPieces(scrollbar, rect);
}

void ScrollbarGeometryStub::splitTrack(WebScrollbar* scrollbar, const WebRect& unconstrainedTrackRect, WebRect& beforeThumbRect, WebRect& thumbRect, WebRect& afterThumbRect)
{
    m_geometry->splitTrack(scrollbar, unconstrainedTrackRect, beforeThumbRect, thumbRect, afterThumbRect);
}

}  // namespace cc

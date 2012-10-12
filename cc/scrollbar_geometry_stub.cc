// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCScrollbarGeometryStub.h"

using WebKit::WebRect;
using WebKit::WebScrollbar;
using WebKit::WebScrollbarThemeGeometry;

namespace cc {

CCScrollbarGeometryStub::CCScrollbarGeometryStub(PassOwnPtr<WebScrollbarThemeGeometry> geometry)
    : m_geometry(geometry)
{
}

CCScrollbarGeometryStub::~CCScrollbarGeometryStub()
{
}

WebScrollbarThemeGeometry* CCScrollbarGeometryStub::clone() const
{
    return m_geometry->clone();
}

int CCScrollbarGeometryStub::thumbPosition(WebScrollbar* scrollbar)
{
    return m_geometry->thumbPosition(scrollbar);
}

int CCScrollbarGeometryStub::thumbLength(WebScrollbar* scrollbar)
{
    return m_geometry->thumbLength(scrollbar);
}

int CCScrollbarGeometryStub::trackPosition(WebScrollbar* scrollbar)
{
    return m_geometry->trackPosition(scrollbar);
}

int CCScrollbarGeometryStub::trackLength(WebScrollbar* scrollbar)
{
    return m_geometry->trackLength(scrollbar);
}

bool CCScrollbarGeometryStub::hasButtons(WebScrollbar* scrollbar)
{
    return m_geometry->hasButtons(scrollbar);
}

bool CCScrollbarGeometryStub::hasThumb(WebScrollbar* scrollbar)
{
    return m_geometry->hasThumb(scrollbar);
}

WebRect CCScrollbarGeometryStub::trackRect(WebScrollbar* scrollbar)
{
    return m_geometry->trackRect(scrollbar);
}

WebRect CCScrollbarGeometryStub::thumbRect(WebScrollbar* scrollbar)
{
    return m_geometry->thumbRect(scrollbar);
}

int CCScrollbarGeometryStub::minimumThumbLength(WebScrollbar* scrollbar)
{
    return m_geometry->minimumThumbLength(scrollbar);
}

int CCScrollbarGeometryStub::scrollbarThickness(WebScrollbar* scrollbar)
{
    return m_geometry->scrollbarThickness(scrollbar);
}

WebRect CCScrollbarGeometryStub::backButtonStartRect(WebScrollbar* scrollbar)
{
    return m_geometry->backButtonStartRect(scrollbar);
}

WebRect CCScrollbarGeometryStub::backButtonEndRect(WebScrollbar* scrollbar)
{
    return m_geometry->backButtonEndRect(scrollbar);
}

WebRect CCScrollbarGeometryStub::forwardButtonStartRect(WebScrollbar* scrollbar)
{
    return m_geometry->forwardButtonStartRect(scrollbar);
}

WebRect CCScrollbarGeometryStub::forwardButtonEndRect(WebScrollbar* scrollbar)
{
    return m_geometry->forwardButtonEndRect(scrollbar);
}

WebRect CCScrollbarGeometryStub::constrainTrackRectToTrackPieces(WebScrollbar* scrollbar, const WebRect& rect)
{
    return m_geometry->constrainTrackRectToTrackPieces(scrollbar, rect);
}

void CCScrollbarGeometryStub::splitTrack(WebScrollbar* scrollbar, const WebRect& unconstrainedTrackRect, WebRect& beforeThumbRect, WebRect& thumbRect, WebRect& afterThumbRect)
{
    m_geometry->splitTrack(scrollbar, unconstrainedTrackRect, beforeThumbRect, thumbRect, afterThumbRect);
}

}

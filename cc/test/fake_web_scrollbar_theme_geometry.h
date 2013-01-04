// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_WEB_SCROLLBAR_THEME_GEOMETRY_H_
#define CC_TEST_FAKE_WEB_SCROLLBAR_THEME_GEOMETRY_H_

#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebScrollbarThemeGeometry.h"

namespace cc {

class FakeWebScrollbarThemeGeometry : public WebKit::WebScrollbarThemeGeometry {
public:
    static scoped_ptr<WebKit::WebScrollbarThemeGeometry> create(bool hasThumb) { return scoped_ptr<WebKit::WebScrollbarThemeGeometry>(new FakeWebScrollbarThemeGeometry(hasThumb)); }

    virtual WebKit::WebScrollbarThemeGeometry* clone() const OVERRIDE;

    virtual int thumbPosition(WebKit::WebScrollbar*) OVERRIDE;
    virtual int thumbLength(WebKit::WebScrollbar*) OVERRIDE;
    virtual int trackPosition(WebKit::WebScrollbar*) OVERRIDE;
    virtual int trackLength(WebKit::WebScrollbar*) OVERRIDE;
    virtual bool hasButtons(WebKit::WebScrollbar*) OVERRIDE;
    virtual bool hasThumb(WebKit::WebScrollbar*) OVERRIDE;
    virtual WebKit::WebRect trackRect(WebKit::WebScrollbar*) OVERRIDE;
    virtual WebKit::WebRect thumbRect(WebKit::WebScrollbar*) OVERRIDE;
    virtual int minimumThumbLength(WebKit::WebScrollbar*) OVERRIDE;
    virtual int scrollbarThickness(WebKit::WebScrollbar*) OVERRIDE;
    virtual WebKit::WebRect backButtonStartRect(WebKit::WebScrollbar*) OVERRIDE;
    virtual WebKit::WebRect backButtonEndRect(WebKit::WebScrollbar*) OVERRIDE;
    virtual WebKit::WebRect forwardButtonStartRect(WebKit::WebScrollbar*) OVERRIDE;
    virtual WebKit::WebRect forwardButtonEndRect(WebKit::WebScrollbar*) OVERRIDE;
    virtual WebKit::WebRect constrainTrackRectToTrackPieces(WebKit::WebScrollbar*, const WebKit::WebRect&) OVERRIDE;
    virtual void splitTrack(WebKit::WebScrollbar*, const WebKit::WebRect& track, WebKit::WebRect& startTrack, WebKit::WebRect& thumb, WebKit::WebRect& endTrack) OVERRIDE;

protected:
    FakeWebScrollbarThemeGeometry(bool hasThumb) : m_hasThumb(hasThumb) { }
    bool m_hasThumb;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_WEB_SCROLLBAR_THEME_GEOMETRY_H_

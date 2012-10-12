// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCScrollbarGeometryStub_h
#define CCScrollbarGeometryStub_h

#include <public/WebScrollbarThemeGeometry.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace cc {

// This subclass wraps an existing scrollbar geometry class so that
// another class can derive from it and override specific functions, while
// passing through the remaining ones.
class CCScrollbarGeometryStub : public WebKit::WebScrollbarThemeGeometry {
public:
    virtual ~CCScrollbarGeometryStub();

    // Allow derived classes to update themselves from a scrollbar.
    void update(WebKit::WebScrollbar*) { }

    // WebScrollbarThemeGeometry interface
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
    explicit CCScrollbarGeometryStub(PassOwnPtr<WebKit::WebScrollbarThemeGeometry>);

private:
    OwnPtr<WebKit::WebScrollbarThemeGeometry> m_geometry;
};

}

#endif

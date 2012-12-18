// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCROLLBAR_GEOMETRY_STUB_H_
#define CC_SCROLLBAR_GEOMETRY_STUB_H_

#include "base/memory/scoped_ptr.h"
#include "cc/cc_export.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebScrollbarThemeGeometry.h"

namespace cc {

// This subclass wraps an existing scrollbar geometry class so that
// another class can derive from it and override specific functions, while
// passing through the remaining ones.
class CC_EXPORT ScrollbarGeometryStub : public NON_EXPORTED_BASE(WebKit::WebScrollbarThemeGeometry) {
public:
    virtual ~ScrollbarGeometryStub();

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
    explicit ScrollbarGeometryStub(scoped_ptr<WebKit::WebScrollbarThemeGeometry>);

private:
    scoped_ptr<WebKit::WebScrollbarThemeGeometry> m_geometry;

    DISALLOW_COPY_AND_ASSIGN(ScrollbarGeometryStub);
};

}

#endif  // CC_SCROLLBAR_GEOMETRY_STUB_H_

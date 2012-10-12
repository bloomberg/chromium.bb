// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCScrollbarGeometryFixedThumb_h
#define CCScrollbarGeometryFixedThumb_h

#include "CCScrollbarGeometryStub.h"
#include "IntSize.h"

namespace cc {

// This scrollbar geometry class behaves exactly like a normal geometry except
// it always returns a fixed thumb length. This allows a page to zoom (changing
// the total size of the scrollable area, changing the thumb length) while not
// requiring the thumb resource to be repainted.
class CCScrollbarGeometryFixedThumb : public CCScrollbarGeometryStub {
public:
    static PassOwnPtr<CCScrollbarGeometryFixedThumb> create(PassOwnPtr<WebKit::WebScrollbarThemeGeometry>);
    virtual ~CCScrollbarGeometryFixedThumb();

    // Update thumb length from scrollbar
    void update(WebKit::WebScrollbar*);

    // WebScrollbarThemeGeometry interface
    virtual WebKit::WebScrollbarThemeGeometry* clone() const OVERRIDE;
    virtual int thumbLength(WebKit::WebScrollbar*) OVERRIDE;
    virtual int thumbPosition(WebKit::WebScrollbar*) OVERRIDE;
    virtual void splitTrack(WebKit::WebScrollbar*, const WebKit::WebRect& track, WebKit::WebRect& startTrack, WebKit::WebRect& thumb, WebKit::WebRect& endTrack) OVERRIDE;

private:
    explicit CCScrollbarGeometryFixedThumb(PassOwnPtr<WebKit::WebScrollbarThemeGeometry>);

    IntSize m_thumbSize;
};

}

#endif

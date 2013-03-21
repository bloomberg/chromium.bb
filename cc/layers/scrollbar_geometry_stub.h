// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_SCROLLBAR_GEOMETRY_STUB_H_
#define CC_LAYERS_SCROLLBAR_GEOMETRY_STUB_H_

#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebScrollbarThemeGeometry.h"

namespace cc {

// This subclass wraps an existing scrollbar geometry class so that
// another class can derive from it and override specific functions, while
// passing through the remaining ones.
class CC_EXPORT ScrollbarGeometryStub :
    public NON_EXPORTED_BASE(WebKit::WebScrollbarThemeGeometry) {
 public:
  virtual ~ScrollbarGeometryStub();

  // Allow derived classes to update themselves from a scrollbar.
  void Update(WebKit::WebScrollbar* scrollbar) {}

  // WebScrollbarThemeGeometry interface
  virtual WebKit::WebScrollbarThemeGeometry* clone() const OVERRIDE;
  virtual int thumbPosition(WebKit::WebScrollbar* scrollbar) OVERRIDE;
  virtual int thumbLength(WebKit::WebScrollbar* scrollbar) OVERRIDE;
  virtual int trackPosition(WebKit::WebScrollbar* scrollbar) OVERRIDE;
  virtual int trackLength(WebKit::WebScrollbar* scrollbar) OVERRIDE;
  virtual bool hasButtons(WebKit::WebScrollbar* scrollbar) OVERRIDE;
  virtual bool hasThumb(WebKit::WebScrollbar* scrollbar) OVERRIDE;
  virtual WebKit::WebRect trackRect(WebKit::WebScrollbar* scrollbar) OVERRIDE;
  virtual WebKit::WebRect thumbRect(WebKit::WebScrollbar* scrollbar) OVERRIDE;
  virtual int minimumThumbLength(WebKit::WebScrollbar* scrollbar) OVERRIDE;
  virtual int scrollbarThickness(WebKit::WebScrollbar* scrollbar) OVERRIDE;
  virtual WebKit::WebRect backButtonStartRect(WebKit::WebScrollbar* scrollbar)
      OVERRIDE;
  virtual WebKit::WebRect backButtonEndRect(WebKit::WebScrollbar* scrollbar)
      OVERRIDE;
  virtual WebKit::WebRect forwardButtonStartRect(
      WebKit::WebScrollbar* scrollbar) OVERRIDE;
  virtual WebKit::WebRect forwardButtonEndRect(WebKit::WebScrollbar* scrollbar)
      OVERRIDE;
  virtual WebKit::WebRect constrainTrackRectToTrackPieces(
      WebKit::WebScrollbar* scrollbar,
      const WebKit::WebRect& rect) OVERRIDE;
  virtual void splitTrack(WebKit::WebScrollbar* scrollbar,
                          const WebKit::WebRect& track,
                          WebKit::WebRect& start_track,
                          WebKit::WebRect& thumb,
                          WebKit::WebRect& end_track) OVERRIDE;

 protected:
  explicit ScrollbarGeometryStub(
      scoped_ptr<WebKit::WebScrollbarThemeGeometry> scrollbar);

 private:
  scoped_ptr<WebKit::WebScrollbarThemeGeometry> geometry_;

  DISALLOW_COPY_AND_ASSIGN(ScrollbarGeometryStub);
};

}

#endif  // CC_LAYERS_SCROLLBAR_GEOMETRY_STUB_H_

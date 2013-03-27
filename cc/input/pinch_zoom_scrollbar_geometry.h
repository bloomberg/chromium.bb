// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_PINCH_ZOOM_SCROLLBAR_GEOMETRY_H_
#define CC_INPUT_PINCH_ZOOM_SCROLLBAR_GEOMETRY_H_

#include "base/basictypes.h"
#include "cc/base/cc_export.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebScrollbarThemeGeometry.h"

using WebKit::WebScrollbar;
using WebKit::WebRect;
using WebKit::WebScrollbarThemeGeometry;

namespace cc {

class PinchZoomScrollbarGeometry : public WebScrollbarThemeGeometry {
 public:
  PinchZoomScrollbarGeometry() {}
  virtual ~PinchZoomScrollbarGeometry() {}

  static const int kTrackWidth;

  // Implement WebScrollbarThemeGeometry interface.
  virtual WebScrollbarThemeGeometry* clone() const;
  virtual int thumbPosition(WebScrollbar* scrollbar);
  virtual int thumbLength(WebScrollbar* scrollbar);
  virtual int trackPosition(WebScrollbar* scrollbar);
  virtual int trackLength(WebScrollbar* scrollbar);
  virtual bool hasButtons(WebScrollbar* scrollbar);
  virtual bool hasThumb(WebScrollbar* scrollbar);
  virtual WebRect trackRect(WebScrollbar* scrollbar);
  virtual WebRect thumbRect(WebScrollbar* scrollbar);
  virtual int minimumThumbLength(WebScrollbar* scrollbar);
  virtual int scrollbarThickness(WebScrollbar* scrollbar);
  virtual WebRect backButtonStartRect(WebScrollbar* scrollbar);
  virtual WebRect backButtonEndRect(WebScrollbar* scrollbar);
  virtual WebRect forwardButtonStartRect(WebScrollbar* scrollbar);
  virtual WebRect forwardButtonEndRect(WebScrollbar* scrollbar);

  virtual WebRect constrainTrackRectToTrackPieces(WebScrollbar* scrollbar,
    const WebRect& trackRect);

  virtual void splitTrack(
    WebScrollbar* scrollbar, const WebRect& track, WebRect& start_track,
    WebRect& thumb, WebRect& end_track);

 private:
  DISALLOW_COPY_AND_ASSIGN(PinchZoomScrollbarGeometry);
};

}  // namespace WebKit
#endif  // CC_INPUT_PINCH_ZOOM_SCROLLBAR_GEOMETRY_H_

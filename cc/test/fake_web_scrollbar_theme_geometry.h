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
  static scoped_ptr<WebKit::WebScrollbarThemeGeometry> Create(bool has_thumb) {
    return scoped_ptr<WebKit::WebScrollbarThemeGeometry>(
        new FakeWebScrollbarThemeGeometry(has_thumb));
  }

  // WebScrollbarThemeGeometry implementation.
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
  FakeWebScrollbarThemeGeometry(bool has_thumb) : has_thumb_(has_thumb) {}
  bool has_thumb_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_WEB_SCROLLBAR_THEME_GEOMETRY_H_

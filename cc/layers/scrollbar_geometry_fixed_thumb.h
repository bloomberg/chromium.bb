// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_SCROLLBAR_GEOMETRY_FIXED_THUMB_H_
#define CC_LAYERS_SCROLLBAR_GEOMETRY_FIXED_THUMB_H_

#include "cc/base/cc_export.h"
#include "cc/layers/scrollbar_geometry_stub.h"
#include "ui/gfx/size.h"

namespace cc {

// This scrollbar geometry class behaves exactly like a normal geometry except
// it always returns a fixed thumb length. This allows a page to zoom (changing
// the total size of the scrollable area, changing the thumb length) while not
// requiring the thumb resource to be repainted.
class CC_EXPORT ScrollbarGeometryFixedThumb : public ScrollbarGeometryStub {
 public:
  static scoped_ptr<ScrollbarGeometryFixedThumb> Create(
      scoped_ptr<WebKit::WebScrollbarThemeGeometry> geometry);
  virtual ~ScrollbarGeometryFixedThumb();

  void set_thumb_size(gfx::Size size) { thumb_size_ = size; }

  // WebScrollbarThemeGeometry interface
  virtual WebKit::WebScrollbarThemeGeometry* clone() const OVERRIDE;
  virtual int thumbLength(WebKit::WebScrollbar* scrollbar) OVERRIDE;
  virtual int thumbPosition(WebKit::WebScrollbar* scrollbar) OVERRIDE;
  virtual void splitTrack(WebKit::WebScrollbar* scrollbar,
                          const WebKit::WebRect& track,
                          WebKit::WebRect& start_track,
                          WebKit::WebRect& thumb,
                          WebKit::WebRect& end_track) OVERRIDE;

 private:
  explicit ScrollbarGeometryFixedThumb(
      scoped_ptr<WebKit::WebScrollbarThemeGeometry> geometry);

  gfx::Size thumb_size_;
};

}

#endif  // CC_LAYERS_SCROLLBAR_GEOMETRY_FIXED_THUMB_H_

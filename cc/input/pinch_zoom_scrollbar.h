// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_PINCH_ZOOM_SCROLLBAR_H_
#define CC_INPUT_PINCH_ZOOM_SCROLLBAR_H_

#include "cc/base/cc_export.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebScrollbar.h"

namespace cc {

class LayerTreeHost;

class PinchZoomScrollbar : public WebKit::WebScrollbar {
 public:
  static const float kDefaultOpacity;
  static const float kFadeDurationInSeconds;

  PinchZoomScrollbar(
      WebKit::WebScrollbar::Orientation orientation,
      LayerTreeHost* owner);
  virtual ~PinchZoomScrollbar() {}

  // Implement WebKit::WebScrollbar.
  virtual bool isOverlay() const;
  virtual int value() const;
  virtual WebKit::WebPoint location() const;
  virtual WebKit::WebSize size() const;
  virtual bool enabled() const;
  virtual int maximum() const;
  virtual int totalSize() const;
  virtual bool isScrollViewScrollbar() const;
  virtual bool isScrollableAreaActive() const;
  virtual void getTickmarks(WebKit::WebVector<WebKit::WebRect>&) const {}
  virtual WebKit::WebScrollbar::ScrollbarControlSize controlSize() const;
  virtual WebKit::WebScrollbar::ScrollbarPart pressedPart() const;
  virtual WebKit::WebScrollbar::ScrollbarPart hoveredPart() const;
  virtual WebKit::WebScrollbar::ScrollbarOverlayStyle
      scrollbarOverlayStyle() const;
  virtual bool isCustomScrollbar() const;
  virtual WebKit::WebScrollbar::Orientation orientation() const;

 private:
  WebKit::WebScrollbar::Orientation orientation_;
  LayerTreeHost* owner_;

  DISALLOW_COPY_AND_ASSIGN(PinchZoomScrollbar);
};  // class PinchZoomScrollbar

}  // namespace cc
#endif  // CC_INPUT_PINCH_ZOOM_SCROLLBAR_H_

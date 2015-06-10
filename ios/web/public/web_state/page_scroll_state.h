// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_PAGE_SCROLL_STATE_H_
#define IOS_WEB_PUBLIC_WEB_STATE_PAGE_SCROLL_STATE_H_

namespace web {

// Class used to represent the scrolling offset and the zoom scale of a webview.
class PageScrollState {
 public:
  // Default constructor.  Initializes scroll offsets and zoom scales to NAN.
  PageScrollState();
  // Constructor with initial values.
  PageScrollState(double scroll_offset_x,
                  double scroll_offset_y,
                  double minimum_zoom_scale,
                  double maximum_zoom_scale,
                  double zoom_scale);
  ~PageScrollState();

  // PageScrollStates cannot be applied until the scroll offset and zoom scale
  // are both valid.
  bool IsValid() const;

  // The scroll offset is valid if its x and y values are both non-NAN.
  bool IsScrollOffsetValid() const;

  // Non-legacy zoom scales are valid if all three values are non-NAN and the
  // zoom scale is within the minimum and maximum scales.  Legacy-format
  // PageScrollStates are considered valid if the minimum and maximum scales
  // are NAN and the zoom scale is greater than zero.
  bool IsZoomScaleValid() const;

  // PageScrollStates restored from the legacy serialization format make
  // assumptions about the web view's implementation of zooming, and contain a
  // non-NAN zoom scale and a NAN minimum and maximum scale.  Legacy zoom scales
  // can only be applied to CRWUIWebViewWebControllers.
  bool IsZoomScaleLegacyFormat() const;

  // Returns the allowed zoom scale range for this scroll state.
  double GetMinMaxZoomDifference() const {
    return maximum_zoom_scale_ - minimum_zoom_scale_;
  }

  // Accessors for scroll offsets and zoom scale.
  double scroll_offset_x() const { return scroll_offset_x_; }
  void set_scroll_offset_x(double scroll_offset_x) {
    scroll_offset_x_ = scroll_offset_x;
  }
  double scroll_offset_y() const { return scroll_offset_y_; }
  void set_scroll_offset_y(double scroll_offset_y) {
    scroll_offset_y_ = scroll_offset_y;
  }
  double minimum_zoom_scale() const { return minimum_zoom_scale_; }
  void set_minimum_zoom_scale(double minimum_zoom_scale) {
    minimum_zoom_scale_ = minimum_zoom_scale;
  }
  double maximum_zoom_scale() const { return maximum_zoom_scale_; }
  void set_maximum_zoom_scale(double maximum_zoom_scale) {
    maximum_zoom_scale_ = maximum_zoom_scale;
  }
  double zoom_scale() const { return zoom_scale_; }
  void set_zoom_scale(double zoom_scale) { zoom_scale_ = zoom_scale; }

  // Comparator operators.
  bool operator==(const PageScrollState& other) const;
  bool operator!=(const PageScrollState& other) const;

 private:
  double scroll_offset_x_;
  double scroll_offset_y_;
  double minimum_zoom_scale_;
  double maximum_zoom_scale_;
  double zoom_scale_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_WEB_STATE_PAGE_SCROLL_STATE_H_

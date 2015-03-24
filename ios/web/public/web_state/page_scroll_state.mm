// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/web_state/page_scroll_state.h"

#include <cmath>

namespace web {

namespace {
// Returns true if:
// - both |value1| and |value2| are NAN, or
// - |value1| and |value2| are equal non-NAN values.
inline bool StateValuesAreEqual(double value1, double value2) {
  return std::isnan(value1) ? std::isnan(value2) : value1 == value2;
}
}  // namespace

PageScrollState::PageScrollState()
    : scroll_offset_x_(NAN),
      scroll_offset_y_(NAN),
      minimum_zoom_scale_(NAN),
      maximum_zoom_scale_(NAN),
      zoom_scale_(NAN) {
}

PageScrollState::PageScrollState(double scroll_offset_x,
                                 double scroll_offset_y,
                                 double minimum_zoom_scale,
                                 double maximum_zoom_scale,
                                 double zoom_scale)
    : scroll_offset_x_(scroll_offset_x),
      scroll_offset_y_(scroll_offset_y),
      minimum_zoom_scale_(minimum_zoom_scale),
      maximum_zoom_scale_(maximum_zoom_scale),
      zoom_scale_(zoom_scale) {
}

PageScrollState::~PageScrollState() {
}

bool PageScrollState::IsValid() const {
  return IsScrollOffsetValid() && IsZoomScaleValid();
}

bool PageScrollState::IsScrollOffsetValid() const {
  return !std::isnan(scroll_offset_x_) && !std::isnan(scroll_offset_y_);
}

bool PageScrollState::IsZoomScaleValid() const {
  return IsZoomScaleLegacyFormat() ||
         (!std::isnan(minimum_zoom_scale_) &&
          !std::isnan(maximum_zoom_scale_) && !std::isnan(zoom_scale_) &&
          zoom_scale_ >= minimum_zoom_scale_ &&
          zoom_scale_ <= maximum_zoom_scale_);
}

bool PageScrollState::IsZoomScaleLegacyFormat() const {
  return std::isnan(minimum_zoom_scale_) && std::isnan(maximum_zoom_scale_) &&
         zoom_scale_ > 0.0;
}

bool PageScrollState::operator==(const PageScrollState& other) const {
  return StateValuesAreEqual(scroll_offset_x_, other.scroll_offset_x_) &&
         StateValuesAreEqual(scroll_offset_y_, other.scroll_offset_y_) &&
         StateValuesAreEqual(minimum_zoom_scale_, other.minimum_zoom_scale_) &&
         StateValuesAreEqual(maximum_zoom_scale_, other.maximum_zoom_scale_) &&
         StateValuesAreEqual(zoom_scale_, other.zoom_scale_);
}

}  // namespace web

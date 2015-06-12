// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/web_state/page_display_state.h"

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

PageScrollState::PageScrollState() : offset_x_(NAN), offset_y_(NAN) {
}

PageScrollState::PageScrollState(double offset_x, double offset_y)
    : offset_x_(offset_x), offset_y_(offset_y) {
}

PageScrollState::~PageScrollState() {
}

bool PageScrollState::IsValid() const {
  return !std::isnan(offset_x_) && !std::isnan(offset_y_);
}

bool PageScrollState::operator==(const PageScrollState& other) const {
  return StateValuesAreEqual(offset_x_, other.offset_x_) &&
         StateValuesAreEqual(offset_y_, other.offset_y_);
}

bool PageScrollState::operator!=(const PageScrollState& other) const {
  return !(*this == other);
}

PageZoomState::PageZoomState()
    : minimum_zoom_scale_(NAN), maximum_zoom_scale_(NAN), zoom_scale_(NAN) {
}

PageZoomState::PageZoomState(double minimum_zoom_scale,
                             double maximum_zoom_scale,
                             double zoom_scale)
    : minimum_zoom_scale_(minimum_zoom_scale),
      maximum_zoom_scale_(maximum_zoom_scale),
      zoom_scale_(zoom_scale) {
}

PageZoomState::~PageZoomState() {
}

bool PageZoomState::IsValid() const {
  return IsLegacyFormat() ||
         (!std::isnan(minimum_zoom_scale_) &&
          !std::isnan(maximum_zoom_scale_) && !std::isnan(zoom_scale_) &&
          zoom_scale_ >= minimum_zoom_scale_ &&
          zoom_scale_ <= maximum_zoom_scale_);
}

bool PageZoomState::IsLegacyFormat() const {
  return std::isnan(minimum_zoom_scale_) && std::isnan(maximum_zoom_scale_) &&
         zoom_scale_ > 0.0;
}

bool PageZoomState::operator==(const PageZoomState& other) const {
  return StateValuesAreEqual(minimum_zoom_scale_, other.minimum_zoom_scale_) &&
         StateValuesAreEqual(maximum_zoom_scale_, other.maximum_zoom_scale_) &&
         StateValuesAreEqual(zoom_scale_, other.zoom_scale_);
}

bool PageZoomState::operator!=(const PageZoomState& other) const {
  return !(*this == other);
}

PageDisplayState::PageDisplayState() {
}

PageDisplayState::PageDisplayState(const PageScrollState& scroll_state,
                                   const PageZoomState& zoom_state)
    : scroll_state_(scroll_state), zoom_state_(zoom_state) {
}

PageDisplayState::PageDisplayState(double offset_x,
                                   double offset_y,
                                   double minimum_zoom_scale,
                                   double maximum_zoom_scale,
                                   double zoom_scale)
    : scroll_state_(offset_x, offset_y),
      zoom_state_(minimum_zoom_scale, maximum_zoom_scale, zoom_scale) {
}

PageDisplayState::~PageDisplayState() {
}

bool PageDisplayState::IsValid() const {
  return scroll_state_.IsValid() && zoom_state_.IsValid();
}

bool PageDisplayState::operator==(const PageDisplayState& other) const {
  return scroll_state_ == other.scroll_state_ &&
         zoom_state_ == other.zoom_state_;
}

bool PageDisplayState::operator!=(const PageDisplayState& other) const {
  return !(*this == other);
}

}  // namespace web

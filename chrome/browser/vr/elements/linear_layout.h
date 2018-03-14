// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_LINEAR_LAYOUT_H_
#define CHROME_BROWSER_VR_ELEMENTS_LINEAR_LAYOUT_H_

#include "chrome/browser/vr/elements/ui_element.h"

namespace vr {

class LinearLayout : public UiElement {
 public:
  enum Direction { kUp, kDown, kLeft, kRight };

  explicit LinearLayout(Direction direction);
  ~LinearLayout() override;

  void set_margin(float margin) { margin_ = margin; }
  void set_direction(Direction direction) { direction_ = direction; }
  void set_layout_length(float extent) { layout_length = extent; }

  // UiElement overrides.
  bool SizeAndLayOut() override;
  void LayOutChildren() override;

 private:
  bool Horizontal() const;

  // Compute the total extents of all layout-enabled children, including margin.
  // Optionally, an element to exclude may be specified, allowing the layout to
  // compute how much space is left for that element.
  void GetTotalExtent(const UiElement* element_to_exclude,
                      float* major_extent,
                      float* minor_extent) const;

  // Sets the specified element to a size that ensures the overall layout totals
  // its own specified extents.
  bool AdjustResizableElement(UiElement* element_to_resize);

  Direction direction_;
  float margin_ = 0.0f;

  // If non-zero, LinearLayout will look for an element tagged as allowing
  // sizing by its parent, and set that element's size such that the total
  // layout's length is attained.
  float layout_length = 0.0f;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_LINEAR_LAYOUT_H_

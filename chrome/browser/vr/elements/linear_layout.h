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

  void LayOutChildren() override;

 private:
  Direction direction_;
  float margin_ = 0.0f;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_LINEAR_LAYOUT_H_

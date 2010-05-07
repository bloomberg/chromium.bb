// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/pepper_scrollbar_widget.h"

const int PepperScrollbarWidget::kMaxOverlapBetweenPages = 40;

void PepperScrollbarWidget::Paint(Graphics2DDeviceContext* context,
                                  const NPRect& dirty) {
  // TODO(port)
}

void PepperScrollbarWidget::GenerateMeasurements() {
  // TODO(port)
  thickness_ = 0;
  arrow_length_ = 0;
}

bool PepperScrollbarWidget::ShouldSnapBack(const gfx::Point& location) const {
  return false;
}

bool PepperScrollbarWidget::ShouldCenterOnThumb(
    const NPPepperEvent& event) const {
  // TODO: to do this properly on Mac, the AppleScrollerPagingBehavior
  // preference needs to be checked.  See ScrollbarThemeChromiumMac.mm.
  return event.u.mouse.button == NPMouseButton_Left &&
         event.u.mouse.modifier & NPEventModifier_AltKey;
}

int PepperScrollbarWidget::MinimumThumbLength() const {
  return thickness_;
}

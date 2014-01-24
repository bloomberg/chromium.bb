// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/test_popup_controller_common.h"

namespace autofill {

TestPopupControllerCommon::TestPopupControllerCommon(
    const gfx::RectF& element_bounds)
    : PopupControllerCommon(element_bounds, NULL, NULL) {}
TestPopupControllerCommon::~TestPopupControllerCommon() {}

gfx::Display TestPopupControllerCommon::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  return display_;
}

}  // namespace autofill

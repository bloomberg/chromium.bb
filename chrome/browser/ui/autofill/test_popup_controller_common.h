// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_TEST_POPUP_CONTROLLER_COMMON_H_
#define CHROME_BROWSER_UI_AUTOFILL_TEST_POPUP_CONTROLLER_COMMON_H_

#include "chrome/browser/ui/autofill/popup_controller_common.h"

#include "ui/gfx/display.h"

namespace autofill {

class TestPopupControllerCommon : public PopupControllerCommon {
 public:
  explicit TestPopupControllerCommon(const gfx::RectF& element_bounds);
  virtual ~TestPopupControllerCommon();

  void set_display(const gfx::Display& display) { display_ = display; }

 protected:
  // Returns |display_|
  virtual gfx::Display GetDisplayNearestPoint(
      const gfx::Point& point) const OVERRIDE;

 private:
  gfx::Display display_;

  DISALLOW_COPY_AND_ASSIGN(TestPopupControllerCommon);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_TEST_POPUP_CONTROLLER_COMMON_H_

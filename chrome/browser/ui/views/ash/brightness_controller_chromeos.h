// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ASH_BRIGHTNESS_CONTROLLER_CHROMEOS_H_
#define CHROME_BROWSER_UI_VIEWS_ASH_BRIGHTNESS_CONTROLLER_CHROMEOS_H_
#pragma once

#include "ash/system/brightness/brightness_control_delegate.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"

// A class which controls brightness when F6, F7 or a multimedia key for
// brightness is pressed.
class BrightnessController : public ash::BrightnessControlDelegate {
 public:
  BrightnessController() {}
  virtual ~BrightnessController() {}

  // Overridden from ash::BrightnessControlDelegate:
  virtual bool HandleBrightnessDown(
      const ui::Accelerator& accelerator) OVERRIDE;
  virtual bool HandleBrightnessUp(const ui::Accelerator& accelerator) OVERRIDE;
  virtual void SetBrightnessPercent(double percent, bool gradual) OVERRIDE;
  virtual void GetBrightnessPercent(
      const base::Callback<void(double)>& callback) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrightnessController);
};

#endif  // CHROME_BROWSER_UI_VIEWS_ASH_BRIGHTNESS_CONTROLLER_CHROMEOS_H_

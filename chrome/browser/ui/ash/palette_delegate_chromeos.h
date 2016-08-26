// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_PALETTE_DELEGATE_CHROMEOS_H_
#define CHROME_BROWSER_UI_ASH_PALETTE_DELEGATE_CHROMEOS_H_

#include <string>

#include "ash/common/palette_delegate.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/values.h"
#include "ui/events/devices/input_device_event_observer.h"

namespace chromeos {

// A class which allows the Ash palette to perform chrome actions.
class PaletteDelegateChromeOS : public ash::PaletteDelegate,
                                public ui::InputDeviceEventObserver {
 public:
  PaletteDelegateChromeOS();
  ~PaletteDelegateChromeOS() override;

 private:
  // ash::PaletteDelegate:
  void CreateNote() override;
  bool HasNoteApp() override;
  void SetPartialMagnifierState(bool enabled) override;
  void SetStylusStateChangedCallback(
      const OnStylusStateChangedCallback& on_stylus_state_changed) override;
  bool ShouldAutoOpenPalette() override;
  void TakeScreenshot() override;
  void TakePartialScreenshot() override;

  // ui::InputDeviceObserver:
  void OnStylusStateChanged(ui::StylusState state) override;

  OnStylusStateChangedCallback on_stylus_state_changed_;

  DISALLOW_COPY_AND_ASSIGN(PaletteDelegateChromeOS);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_ASH_PALETTE_DELEGATE_CHROMEOS_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_IME_CONTROLLER_CHROMEOS_H_
#define CHROME_BROWSER_UI_ASH_IME_CONTROLLER_CHROMEOS_H_

#include "ash/ime_control_delegate.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"

// A class which controls ime when an IME shortcut key such as Control+space is
// pressed.
class ImeController : public ash::ImeControlDelegate {
 public:
  ImeController() {}
  virtual ~ImeController() {}

  // Overridden from ash::ImeControlDelegate:
  virtual void HandleNextIme() OVERRIDE;
  virtual bool HandlePreviousIme(const ui::Accelerator& accelerator) OVERRIDE;
  virtual bool HandleSwitchIme(const ui::Accelerator& accelerator) OVERRIDE;
  virtual ui::Accelerator RemapAccelerator(
      const ui::Accelerator& accelerator) OVERRIDE;

 private:
  bool UsingFrenchInputMethod() const;

  DISALLOW_COPY_AND_ASSIGN(ImeController);
};

#endif  // CHROME_BROWSER_UI_ASH_IME_CONTROLLER_CHROMEOS_H_

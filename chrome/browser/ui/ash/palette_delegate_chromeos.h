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

namespace chromeos {

// A class which allows the Ash palette to perform chrome actions.
class PaletteDelegateChromeOS : public ash::PaletteDelegate {
 public:
  PaletteDelegateChromeOS();
  ~PaletteDelegateChromeOS() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PaletteDelegateChromeOS);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_ASH_PALETTE_DELEGATE_CHROMEOS_H_

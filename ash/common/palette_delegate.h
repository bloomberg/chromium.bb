// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_PALETTE_DELEGATE_H_
#define ASH_COMMON_PALETTE_DELEGATE_H_

#include "ash/ash_export.h"
#include "base/macros.h"

namespace ash {

// This delegate allows the UI code in ash, e.g. |PaletteTray|, to perform
// Chrome-specific actions.
class PaletteDelegate {
 public:
  virtual ~PaletteDelegate() {}

  // TODO(jdufault): This delegate will be implemented in later CLs.

 private:
  DISALLOW_ASSIGN(PaletteDelegate);
};

}  // namespace ash

#endif  // ASH_COMMON_PALETTE_DELEGATE_H_

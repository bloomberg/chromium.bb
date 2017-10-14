// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PALETTE_DELEGATE_H_
#define ASH_PALETTE_DELEGATE_H_

#include "ash/ash_export.h"
#include "base/callback.h"
#include "base/macros.h"

namespace ash {

// This delegate allows the UI code in ash, e.g. |PaletteTray|, to perform
// Chrome-specific actions.
// TODO(kaznacheev): delete this class.
class PaletteDelegate {
 public:
  virtual ~PaletteDelegate() {}

 private:
  DISALLOW_ASSIGN(PaletteDelegate);
};

}  // namespace ash

#endif  // ASH_PALETTE_DELEGATE_H_

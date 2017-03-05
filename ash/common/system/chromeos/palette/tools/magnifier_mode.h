// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_CHROMEOS_PALETTE_MAGNIFIER_MODE_H_
#define ASH_COMMON_SYSTEM_CHROMEOS_PALETTE_MAGNIFIER_MODE_H_

#include "ash/common/system/chromeos/palette/common_palette_tool.h"

namespace ash {

// Exposes a palette tool that lets the user enable a partial screen magnifier
// (ie, a spyglass) that dynamically appears when pressing the screen.
class MagnifierMode : public CommonPaletteTool {
 public:
  explicit MagnifierMode(Delegate* delegate);
  ~MagnifierMode() override;

 private:
  // PaletteTool overrides.
  PaletteGroup GetGroup() const override;
  PaletteToolId GetToolId() const override;
  const gfx::VectorIcon& GetActiveTrayIcon() const override;
  void OnEnable() override;
  void OnDisable() override;
  views::View* CreateView() override;

  // CommonPaletteTool overrides.
  const gfx::VectorIcon& GetPaletteIcon() const override;

  DISALLOW_COPY_AND_ASSIGN(MagnifierMode);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_CHROMEOS_PALETTE_MAGNIFIER_MODE_H_

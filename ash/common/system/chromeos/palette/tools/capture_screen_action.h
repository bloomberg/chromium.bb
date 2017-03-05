// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_CHROMEOS_PALETTE_CAPTURE_SCREEN_ACTION_H_
#define ASH_COMMON_SYSTEM_CHROMEOS_PALETTE_CAPTURE_SCREEN_ACTION_H_

#include "ash/ash_export.h"
#include "ash/common/system/chromeos/palette/common_palette_tool.h"

namespace ash {

class ASH_EXPORT CaptureScreenAction : public CommonPaletteTool {
 public:
  explicit CaptureScreenAction(Delegate* delegate);
  ~CaptureScreenAction() override;

 private:
  // PaletteTool overrides.
  PaletteGroup GetGroup() const override;
  PaletteToolId GetToolId() const override;
  void OnEnable() override;
  views::View* CreateView() override;

  // CommonPaletteTool overrides.
  const gfx::VectorIcon& GetPaletteIcon() const override;

  DISALLOW_COPY_AND_ASSIGN(CaptureScreenAction);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_CHROMEOS_PALETTE_CAPTURE_SCREEN_ACTION_H_

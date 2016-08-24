// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_CHROMEOS_PALETTE_CAPTURE_REGION_ACTION_H_
#define ASH_COMMON_SYSTEM_CHROMEOS_PALETTE_CAPTURE_REGION_ACTION_H_

#include "ash/ash_export.h"
#include "ash/common/system/chromeos/palette/common_palette_tool.h"

namespace ash {

class ASH_EXPORT CaptureRegionAction : public CommonPaletteTool {
 public:
  explicit CaptureRegionAction(Delegate* delegate);
  ~CaptureRegionAction() override;

 private:
  // PaletteTool:
  PaletteGroup GetGroup() const override;
  PaletteToolId GetToolId() const override;
  void OnEnable() override;
  views::View* CreateView() override;

  // CommonPaletteTool:
  gfx::VectorIconId GetPaletteIconId() override;

  DISALLOW_COPY_AND_ASSIGN(CaptureRegionAction);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_CHROMEOS_PALETTE_CAPTURE_REGION_ACTION_H_

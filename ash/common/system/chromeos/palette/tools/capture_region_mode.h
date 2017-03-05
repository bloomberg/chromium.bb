// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_CHROMEOS_PALETTE_CAPTURE_REGION_ACTION_H_
#define ASH_COMMON_SYSTEM_CHROMEOS_PALETTE_CAPTURE_REGION_ACTION_H_

#include "ash/ash_export.h"
#include "ash/common/system/chromeos/palette/common_palette_tool.h"
#include "base/memory/weak_ptr.h"

namespace ash {

class ASH_EXPORT CaptureRegionMode : public CommonPaletteTool {
 public:
  explicit CaptureRegionMode(Delegate* delegate);
  ~CaptureRegionMode() override;

 private:
  // PaletteTool:
  PaletteGroup GetGroup() const override;
  PaletteToolId GetToolId() const override;
  const gfx::VectorIcon& GetActiveTrayIcon() const override;
  void OnEnable() override;
  void OnDisable() override;
  views::View* CreateView() override;

  // CommonPaletteTool:
  const gfx::VectorIcon& GetPaletteIcon() const override;

  void OnScreenshotDone();

  base::WeakPtrFactory<CaptureRegionMode> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CaptureRegionMode);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_CHROMEOS_PALETTE_CAPTURE_REGION_ACTION_H_

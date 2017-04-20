// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PALETTE_TOOLS_METALAYER_MODE_H_
#define ASH_SYSTEM_PALETTE_TOOLS_METALAYER_MODE_H_

#include "ash/ash_export.h"
#include "ash/system/palette/common_palette_tool.h"
#include "base/memory/weak_ptr.h"

namespace ash {

// A palette tool that lets the user select a screen region to be passed
// to the voice interaction framework.
class ASH_EXPORT MetalayerMode : public CommonPaletteTool {
 public:
  explicit MetalayerMode(Delegate* delegate);
  ~MetalayerMode() override;

 private:
  // PaletteTool:
  PaletteGroup GetGroup() const override;
  PaletteToolId GetToolId() const override;
  void OnEnable() override;
  void OnDisable() override;
  const gfx::VectorIcon& GetActiveTrayIcon() const override;
  views::View* CreateView() override;

  // CommonPaletteTool:
  const gfx::VectorIcon& GetPaletteIcon() const override;

  void OnMetalayerDone();

  base::WeakPtrFactory<MetalayerMode> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MetalayerMode);
};

}  // namespace ash

#endif  // ASH_SYSTEM_PALETTE_TOOLS_METALAYER_MODE_H_

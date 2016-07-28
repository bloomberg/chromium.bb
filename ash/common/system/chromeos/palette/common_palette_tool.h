// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_CHROMEOS_PALETTE_COMMON_PALETTE_TOOL_H_
#define ASH_COMMON_SYSTEM_CHROMEOS_PALETTE_COMMON_PALETTE_TOOL_H_

#include "ash/common/system/chromeos/palette/palette_tool.h"
#include "ash/common/system/tray/hover_highlight_view.h"
#include "ash/common/system/tray/view_click_listener.h"
#include "base/strings/string16.h"
#include "ui/gfx/vector_icons_public.h"

namespace ash {

// A PaletteTool implementation with a standard view support.
class CommonPaletteTool : public PaletteTool, public ash::ViewClickListener {
 protected:
  explicit CommonPaletteTool(Delegate* delegate);
  ~CommonPaletteTool() override;

  // PaletteTool:
  views::View* CreateView() override;
  void OnViewDestroyed() override;
  void OnEnable() override;
  void OnDisable() override;

  // ViewClickListener:
  void OnViewClicked(views::View* sender) override;

  // Returns the icon used in the palette tray on the left-most edge of the
  // tool.
  virtual gfx::VectorIconId GetPaletteIconId() = 0;

  // Creates a default view implementation to be returned by CreateView.
  views::View* CreateDefaultView(const base::string16& name);

 private:
  HoverHighlightView* highlight_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(CommonPaletteTool);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_CHROMEOS_PALETTE_COMMON_PALETTE_TOOL_H_

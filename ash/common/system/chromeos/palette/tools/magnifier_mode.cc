// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/palette/tools/magnifier_mode.h"

#include "ash/common/palette_delegate.h"
#include "ash/common/system/chromeos/palette/palette_ids.h"
#include "ash/common/wm_shell.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

MagnifierMode::MagnifierMode(Delegate* delegate)
    : CommonPaletteTool(delegate) {}

MagnifierMode::~MagnifierMode() {}

PaletteGroup MagnifierMode::GetGroup() const {
  return PaletteGroup::MODE;
}

PaletteToolId MagnifierMode::GetToolId() const {
  return PaletteToolId::MAGNIFY;
}

gfx::VectorIconId MagnifierMode::GetActiveTrayIcon() {
  return gfx::VectorIconId::PALETTE_TRAY_ICON_MAGNIFY;
}

void MagnifierMode::OnEnable() {
  CommonPaletteTool::OnEnable();
  WmShell::Get()->palette_delegate()->SetPartialMagnifierState(true);
}

void MagnifierMode::OnDisable() {
  CommonPaletteTool::OnDisable();
  WmShell::Get()->palette_delegate()->SetPartialMagnifierState(false);
}

views::View* MagnifierMode::CreateView() {
  return CreateDefaultView(
      l10n_util::GetStringUTF16(IDS_ASH_STYLUS_TOOLS_MAGNIFIER_MODE));
}

gfx::VectorIconId MagnifierMode::GetPaletteIconId() {
  return gfx::VectorIconId::PALETTE_MODE_MAGNIFY;
}

}  // namespace ash

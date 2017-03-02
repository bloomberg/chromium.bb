// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/palette/tools/capture_screen_action.h"

#include "ash/common/palette_delegate.h"
#include "ash/common/system/chromeos/palette/palette_ids.h"
#include "ash/common/wm_shell.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

CaptureScreenAction::CaptureScreenAction(Delegate* delegate)
    : CommonPaletteTool(delegate) {}

CaptureScreenAction::~CaptureScreenAction() {}

PaletteGroup CaptureScreenAction::GetGroup() const {
  return PaletteGroup::ACTION;
}

PaletteToolId CaptureScreenAction::GetToolId() const {
  return PaletteToolId::CAPTURE_SCREEN;
}

void CaptureScreenAction::OnEnable() {
  CommonPaletteTool::OnEnable();

  delegate()->DisableTool(GetToolId());
  delegate()->HidePaletteImmediately();
  WmShell::Get()->palette_delegate()->TakeScreenshot();
}

views::View* CaptureScreenAction::CreateView() {
  return CreateDefaultView(
      l10n_util::GetStringUTF16(IDS_ASH_STYLUS_TOOLS_CAPTURE_SCREEN_ACTION));
}

const gfx::VectorIcon& CaptureScreenAction::GetPaletteIcon() const {
  return kPaletteActionCaptureScreenIcon;
}

}  // namespace ash

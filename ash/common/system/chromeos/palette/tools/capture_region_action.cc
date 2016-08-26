// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/palette/tools/capture_region_action.h"

#include "ash/common/accelerators/accelerator_controller.h"
#include "ash/common/palette_delegate.h"
#include "ash/common/system/chromeos/palette/palette_ids.h"
#include "ash/common/system/toast/toast_data.h"
#include "ash/common/system/toast/toast_manager.h"
#include "ash/common/wm_shell.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

const char kToastId[] = "palette_capture_region";
const int kToastDurationMs = 2500;

}  // namespace

CaptureRegionAction::CaptureRegionAction(Delegate* delegate)
    : CommonPaletteTool(delegate) {}

CaptureRegionAction::~CaptureRegionAction() {}

PaletteGroup CaptureRegionAction::GetGroup() const {
  return PaletteGroup::ACTION;
}

PaletteToolId CaptureRegionAction::GetToolId() const {
  return PaletteToolId::CAPTURE_REGION;
}

void CaptureRegionAction::OnEnable() {
  CommonPaletteTool::OnEnable();

  ToastData toast(kToastId, l10n_util::GetStringUTF16(
                                IDS_ASH_STYLUS_TOOLS_CAPTURE_REGION_TOAST),
                  kToastDurationMs, base::string16());
  ash::WmShell::Get()->toast_manager()->Show(toast);

  WmShell::Get()->palette_delegate()->TakePartialScreenshot();
  delegate()->DisableTool(GetToolId());
  delegate()->HidePalette();
}

views::View* CaptureRegionAction::CreateView() {
  return CreateDefaultView(
      l10n_util::GetStringUTF16(IDS_ASH_STYLUS_TOOLS_CAPTURE_REGION_ACTION));
}

gfx::VectorIconId CaptureRegionAction::GetPaletteIconId() {
  return gfx::VectorIconId::PALETTE_ACTION_CAPTURE_REGION;
}

}  // namespace ash

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/palette/tools/capture_region_mode.h"

#include "ash/common/accelerators/accelerator_controller.h"
#include "ash/common/palette_delegate.h"
#include "ash/common/system/chromeos/palette/palette_ids.h"
#include "ash/common/system/toast/toast_data.h"
#include "ash/common/system/toast/toast_manager.h"
#include "ash/common/wm_shell.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

const char kToastId[] = "palette_capture_region";
const int kToastDurationMs = 2500;

}  // namespace

CaptureRegionMode::CaptureRegionMode(Delegate* delegate)
    : CommonPaletteTool(delegate), weak_factory_(this) {}

CaptureRegionMode::~CaptureRegionMode() {}

PaletteGroup CaptureRegionMode::GetGroup() const {
  return PaletteGroup::MODE;
}

PaletteToolId CaptureRegionMode::GetToolId() const {
  return PaletteToolId::CAPTURE_REGION;
}

const gfx::VectorIcon& CaptureRegionMode::GetActiveTrayIcon() const {
  return kPaletteTrayIconCaptureRegionIcon;
}

void CaptureRegionMode::OnEnable() {
  CommonPaletteTool::OnEnable();

  ToastData toast(kToastId, l10n_util::GetStringUTF16(
                                IDS_ASH_STYLUS_TOOLS_CAPTURE_REGION_TOAST),
                  kToastDurationMs, base::Optional<base::string16>());
  ash::WmShell::Get()->toast_manager()->Show(toast);

  WmShell::Get()->palette_delegate()->TakePartialScreenshot(base::Bind(
      &CaptureRegionMode::OnScreenshotDone, weak_factory_.GetWeakPtr()));
  delegate()->HidePalette();
}

void CaptureRegionMode::OnDisable() {
  CommonPaletteTool::OnDisable();

  // If the user manually cancelled the action we need to make sure to cancel
  // the screenshot session as well.
  WmShell::Get()->palette_delegate()->CancelPartialScreenshot();
}

views::View* CaptureRegionMode::CreateView() {
  return CreateDefaultView(
      l10n_util::GetStringUTF16(IDS_ASH_STYLUS_TOOLS_CAPTURE_REGION_ACTION));
}

const gfx::VectorIcon& CaptureRegionMode::GetPaletteIcon() const {
  return kPaletteActionCaptureRegionIcon;
}

void CaptureRegionMode::OnScreenshotDone() {
  // The screenshot finished, so disable the tool.
  delegate()->DisableTool(GetToolId());
}

}  // namespace ash

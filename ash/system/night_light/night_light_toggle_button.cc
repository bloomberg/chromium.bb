// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/night_light/night_light_toggle_button.h"

#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/night_light/night_light_controller.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"

namespace ash {

namespace {

gfx::ImageSkia GetNightLightNormalStateIcon(bool night_light_enabled) {
  if (night_light_enabled)
    return gfx::CreateVectorIcon(kSystemMenuNightLightOnIcon, kMenuIconColor);

  // Use the same icon theme used for inactive items in the tray when
  // NightLight is not active.
  return gfx::CreateVectorIcon(kSystemMenuNightLightOffIcon,
                               TrayPopupItemStyle::GetIconColor(
                                   TrayPopupItemStyle::ColorStyle::INACTIVE));
}

gfx::ImageSkia GetNightLightDisabledStateIcon(bool night_light_enabled) {
  return gfx::CreateVectorIcon(night_light_enabled
                                   ? kSystemMenuNightLightOnIcon
                                   : kSystemMenuNightLightOffIcon,
                               kMenuIconColorDisabled);
}

}  // namespace

NightLightToggleButton::NightLightToggleButton(views::ButtonListener* listener)
    : SystemMenuButton(listener,
                       TrayPopupInkDropStyle::HOST_CENTERED,
                       kSystemMenuNightLightOffIcon,
                       IDS_ASH_STATUS_TRAY_NIGHT_LIGHT) {
  Update();
}

void NightLightToggleButton::Update() {
  const bool night_light_enabled =
      Shell::Get()->night_light_controller()->GetEnabled();

  SetImage(views::Button::STATE_NORMAL,
           GetNightLightNormalStateIcon(night_light_enabled));
  SetImage(views::Button::STATE_DISABLED,
           GetNightLightDisabledStateIcon(night_light_enabled));
}

void NightLightToggleButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->SetName(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NIGHT_LIGHT));
  node_data->role = ui::AX_ROLE_TOGGLE_BUTTON;
  if (Shell::Get()->night_light_controller()->GetEnabled())
    node_data->AddState(ui::AX_STATE_PRESSED);
}

}  // namespace ash

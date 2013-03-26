// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_popup_header_button.h"

#include "ash/ash_constants.h"
#include "ash/system/tray/tray_constants.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"

namespace ash {
namespace internal {

TrayPopupHeaderButton::TrayPopupHeaderButton(views::ButtonListener* listener,
                                             int enabled_resource_id,
                                             int disabled_resource_id,
                                             int enabled_resource_id_hover,
                                             int disabled_resource_id_hover,
                                             int accessible_name_id)
    : views::ToggleImageButton(listener) {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  SetImage(views::Button::STATE_NORMAL,
      bundle.GetImageNamed(enabled_resource_id).ToImageSkia());
  SetToggledImage(views::Button::STATE_NORMAL,
      bundle.GetImageNamed(disabled_resource_id).ToImageSkia());
  SetImage(views::Button::STATE_HOVERED,
      bundle.GetImageNamed(enabled_resource_id_hover).ToImageSkia());
  SetToggledImage(views::Button::STATE_HOVERED,
      bundle.GetImageNamed(disabled_resource_id_hover).ToImageSkia());
  SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                    views::ImageButton::ALIGN_MIDDLE);
  SetAccessibleName(bundle.GetLocalizedString(accessible_name_id));
  set_focusable(true);
  set_request_focus_on_press(false);
}

TrayPopupHeaderButton::~TrayPopupHeaderButton() {}

gfx::Size TrayPopupHeaderButton::GetPreferredSize() {
  return gfx::Size(ash::kTrayPopupItemHeight, ash::kTrayPopupItemHeight);
}

void TrayPopupHeaderButton::OnPaintBorder(gfx::Canvas* canvas) {
  // Just the left border.
  const int kBorderHeight = 25;
  int padding = (height() - kBorderHeight) / 2;
  canvas->FillRect(gfx::Rect(0, padding, 1, height() - padding * 2),
      ash::kBorderDarkColor);
}

void TrayPopupHeaderButton::OnPaintFocusBorder(gfx::Canvas* canvas) {
  if (HasFocus() && (focusable() || IsAccessibilityFocusable())) {
    canvas->DrawRect(gfx::Rect(2, 1, width() - 4, height() - 3),
                     kFocusBorderColor);
  }
}

void TrayPopupHeaderButton::StateChanged() {
  SchedulePaint();
}

}  // namespace internal
}  // namespace ash

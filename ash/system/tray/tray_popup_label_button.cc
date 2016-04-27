// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_popup_label_button.h"

#include "ash/ash_constants.h"
#include "ash/system/tray/tray_popup_label_button_border.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/painter.h"

namespace ash {

TrayPopupLabelButton::TrayPopupLabelButton(views::ButtonListener* listener,
                                           const base::string16& text)
    : views::LabelButton(listener, text) {
  SetBorder(std::unique_ptr<views::Border>(new TrayPopupLabelButtonBorder));
  SetFocusBehavior(FocusBehavior::ALWAYS);
  set_request_focus_on_press(false);
  set_animate_on_state_change(false);
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  SetFocusPainter(views::Painter::CreateSolidFocusPainter(
      kFocusBorderColor,
      gfx::Insets(1, 1, 2, 2)));
}

TrayPopupLabelButton::~TrayPopupLabelButton() {}

}  // namespace ash

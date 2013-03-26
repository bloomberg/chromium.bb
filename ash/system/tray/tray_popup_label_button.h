// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_POPUP_LABEL_BUTTON_H_
#define ASH_SYSTEM_TRAY_TRAY_POPUP_LABEL_BUTTON_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/string16.h"
#include "ui/views/controls/button/label_button.h"

namespace ash {
namespace internal {

// A label button with custom alignment, border and focus border.
class TrayPopupLabelButton : public views::LabelButton {
 public:
  TrayPopupLabelButton(views::ButtonListener* listener, const string16& text);
  virtual ~TrayPopupLabelButton();

 private:
  // Overridden from views::LabelButton:
  virtual void OnPaintFocusBorder(gfx::Canvas* canvas) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(TrayPopupLabelButton);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_POPUP_LABEL_BUTTON_H_

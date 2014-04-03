// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_POPUP_LABEL_BUTTON_BORDER_H_
#define ASH_SYSTEM_TRAY_TRAY_POPUP_LABEL_BUTTON_BORDER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/controls/button/label_button_border.h"

namespace ash {

// A border for label buttons that paints a vertical separator in normal state
// and a custom hover effect in hovered or pressed state.
class TrayPopupLabelButtonBorder : public views::LabelButtonBorder {
 public:
  TrayPopupLabelButtonBorder();
  virtual ~TrayPopupLabelButtonBorder();

  // views::LabelButtonBorder:
  virtual void Paint(const views::View& view, gfx::Canvas* canvas) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(TrayPopupLabelButtonBorder);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_POPUP_LABEL_BUTTON_BORDER_H_

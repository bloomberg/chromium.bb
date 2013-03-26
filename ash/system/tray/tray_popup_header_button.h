// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_POPUP_HEADER_BUTTON_H_
#define ASH_SYSTEM_TRAY_TRAY_POPUP_HEADER_BUTTON_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/controls/button/image_button.h"

namespace ash {
namespace internal {

// A ToggleImageButton with fixed size, paddings and hover effects. These
// buttons are used in the header.
class TrayPopupHeaderButton : public views::ToggleImageButton {
 public:
  TrayPopupHeaderButton(views::ButtonListener* listener,
                        int enabled_resource_id,
                        int disabled_resource_id,
                        int enabled_resource_id_hover,
                        int disabled_resource_id_hover,
                        int accessible_name_id);
  virtual ~TrayPopupHeaderButton();

 private:
  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void OnPaintBorder(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnPaintFocusBorder(gfx::Canvas* canvas) OVERRIDE;

  // Overridden from views::CustomButton:
  virtual void StateChanged() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(TrayPopupHeaderButton);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_POPUP_HEADER_BUTTON_H_

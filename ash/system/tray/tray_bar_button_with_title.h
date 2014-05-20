// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_BAR_BUTTON_WITH_TITLE_H_
#define ASH_SYSTEM_TRAY_TRAY_BAR_BUTTON_WITH_TITLE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/controls/button/custom_button.h"

namespace views {
class Label;
}

namespace ash {

// A button with a bar image and title text below the bar image. These buttons
// will be used in audio and brightness control UI, which can be toggled with
// on/off states.
class TrayBarButtonWithTitle : public views::CustomButton {
 public:
  TrayBarButtonWithTitle(views::ButtonListener* listener,
                         int title_id,
                         int width);
  virtual ~TrayBarButtonWithTitle();

  void UpdateButton(bool control_on);

 private:
  class TrayBarButton;

  // Overridden from views::CustomButton:
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual void Layout() OVERRIDE;

  TrayBarButton* image_;
  views::Label* title_;
  int width_;
  int image_height_;

  DISALLOW_COPY_AND_ASSIGN(TrayBarButtonWithTitle);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_BAR_BUTTON_WITH_TITLE_H_

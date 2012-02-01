// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_VOLUME_MENU_BUTTON_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_VOLUME_MENU_BUTTON_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/status/status_area_button.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/controls/menu/view_menu_delegate.h"

namespace chromeos {

// The volume button in the status area.
class VolumeMenuButton : public StatusAreaButton,
                         public views::MenuDelegate,
                         public views::ViewMenuDelegate {
 public:
  explicit VolumeMenuButton(StatusAreaButton::Delegate* delegate);
  virtual ~VolumeMenuButton();

  // Update the volume icon.
  void UpdateIcon();

 protected:
  // StatusAreaButton implementation.
  virtual int icon_width() OVERRIDE;

 private:
  // views::View implementation.
  virtual void OnLocaleChanged() OVERRIDE;

  // views::ViewMenuDelegate implementation.
  virtual void RunMenu(views::View* source, const gfx::Point& pt) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(VolumeMenuButton);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_VOLUME_MENU_BUTTON_H_

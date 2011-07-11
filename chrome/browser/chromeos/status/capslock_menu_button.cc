// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/capslock_menu_button.h"

#include <string>

#include "chrome/browser/chromeos/input_method/xkeyboard.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// CapslockMenuButton

CapslockMenuButton::CapslockMenuButton(StatusAreaHost* host)
    : StatusAreaButton(host, this) {
  SetTooltipText(UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_STATUSBAR_CAPSLOCK_ENABLED)));
  SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_STATUSBAR_CAPSLOCK_ENABLED));
  UpdateUIFromCurrentCapslock(input_method::CapsLockIsEnabled());
  SystemKeyEventListener::GetInstance()->AddCapslockObserver(this);
}

CapslockMenuButton::~CapslockMenuButton() {
  SystemKeyEventListener::GetInstance()->RemoveCapslockObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// views::View implementation:

gfx::Size CapslockMenuButton::GetPreferredSize() {
  return StatusAreaButton::GetPreferredSize();
}

void CapslockMenuButton::OnLocaleChanged() {
  UpdateUIFromCurrentCapslock(input_method::CapsLockIsEnabled());
}

////////////////////////////////////////////////////////////////////////////////
// views::ViewMenuDelegate implementation:

void CapslockMenuButton::RunMenu(views::View* unused_source,
                                 const gfx::Point& pt) {
  // This button is just an indicator, and therefore does not have a menu.
}

////////////////////////////////////////////////////////////////////////////////
// SystemKeyEventListener::CapslockObserver implementation

void CapslockMenuButton::OnCapslockChange(bool enabled) {
  UpdateUIFromCurrentCapslock(enabled);
}

void CapslockMenuButton::UpdateUIFromCurrentCapslock(bool enabled) {
  if (enabled) {
    SetIcon(*ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_STATUSBAR_CAPSLOCK));
  } else {
    SetIcon(SkBitmap());
  }
  SetEnabled(enabled);
  Layout();
  SchedulePaint();
}

}  // namespace chromeos

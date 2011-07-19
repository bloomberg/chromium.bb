// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/caps_lock_menu_button.h"

#include <string>

#include "chrome/browser/chromeos/input_method/xkeyboard.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// CapsLockMenuButton

CapsLockMenuButton::CapsLockMenuButton(StatusAreaHost* host)
    : StatusAreaButton(host, this) {
  SetIcon(*ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_STATUSBAR_CAPS_LOCK));
  SetTooltipText(UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_STATUSBAR_CAPS_LOCK_ENABLED)));
  SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_STATUSBAR_CAPS_LOCK_ENABLED));
  UpdateUIFromCurrentCapsLock(input_method::CapsLockIsEnabled());
  SystemKeyEventListener::GetInstance()->AddCapsLockObserver(this);
}

CapsLockMenuButton::~CapsLockMenuButton() {
  SystemKeyEventListener::GetInstance()->RemoveCapsLockObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// views::View implementation:

void CapsLockMenuButton::OnLocaleChanged() {
  UpdateUIFromCurrentCapsLock(input_method::CapsLockIsEnabled());
}

////////////////////////////////////////////////////////////////////////////////
// views::ViewMenuDelegate implementation:

void CapsLockMenuButton::RunMenu(views::View* unused_source,
                                 const gfx::Point& pt) {
  // This button is just an indicator, and therefore does not have a menu.
}

////////////////////////////////////////////////////////////////////////////////
// SystemKeyEventListener::CapsLockObserver implementation

void CapsLockMenuButton::OnCapsLockChange(bool enabled) {
  UpdateUIFromCurrentCapsLock(enabled);
}

void CapsLockMenuButton::UpdateUIFromCurrentCapsLock(bool enabled) {
  SetVisible(enabled);
}

}  // namespace chromeos

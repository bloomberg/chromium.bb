// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/capslock_menu_button.h"

#include <string>
#include <X11/XKBlib.h>

#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/x/x11_util.h"

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// CapslockMenuButton

CapslockMenuButton::CapslockMenuButton(StatusAreaHost* host)
    : StatusAreaButton(host, this) {
  SetTooltipText(UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_STATUSBAR_CAPSLOCK_ENABLED)));
  SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_STATUSBAR_CAPSLOCK_ENABLED));
  UpdateUIFromCurrentCapslock();
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
  UpdateUIFromCurrentCapslock();
}

////////////////////////////////////////////////////////////////////////////////
// views::ViewMenuDelegate implementation:

void CapslockMenuButton::RunMenu(views::View* unused_source,
                                 const gfx::Point& pt) {
  // This button is just an indicator, and therefore does not have a menu.
}

////////////////////////////////////////////////////////////////////////////////
// SystemKeyEventListener::CapslockObserver implementation

void CapslockMenuButton::OnCapslockChange() {
  UpdateUIFromCurrentCapslock();
}

void CapslockMenuButton::UpdateUIFromCurrentCapslock() {
  XkbStateRec status;
  XkbGetState(ui::GetXDisplay(), XkbUseCoreKbd, &status);
  bool enabled = status.locked_mods & LockMask;

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

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/caps_lock_menu_button.h"

#include <string>

#include "chrome/browser/chromeos/input_method/xkeyboard.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// Returns PrefService object associated with |host|.
PrefService* GetPrefService(chromeos::StatusAreaHost* host) {
  if (host->GetProfile())
    return host->GetProfile()->GetPrefs();
  return NULL;
}

}  // namespace

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// CapsLockMenuButton

CapsLockMenuButton::CapsLockMenuButton(StatusAreaHost* host)
    : StatusAreaButton(host, this),
      prefs_(GetPrefService(host)) {
  if (prefs_)
    remap_search_key_to_.Init(
        prefs::kLanguageXkbRemapSearchKeyTo, prefs_, this);

  SetIcon(*ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_STATUSBAR_CAPS_LOCK));
  UpdateTooltip();
  UpdateUIFromCurrentCapsLock(input_method::XKeyboard::CapsLockIsEnabled());
  if (SystemKeyEventListener::GetInstance())
    SystemKeyEventListener::GetInstance()->AddCapsLockObserver(this);
}

CapsLockMenuButton::~CapsLockMenuButton() {
  if (SystemKeyEventListener::GetInstance())
    SystemKeyEventListener::GetInstance()->RemoveCapsLockObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// views::View implementation:

void CapsLockMenuButton::OnLocaleChanged() {
  UpdateUIFromCurrentCapsLock(input_method::XKeyboard::CapsLockIsEnabled());
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

////////////////////////////////////////////////////////////////////////////////
// NotificationObserver implementation

void CapsLockMenuButton::Observe(int type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_PREF_CHANGED)
    UpdateTooltip();
}

void CapsLockMenuButton::UpdateTooltip() {
  int id = IDS_STATUSBAR_CAPS_LOCK_ENABLED;
  if (prefs_ && (remap_search_key_to_.GetValue() == input_method::kCapsLockKey))
    id = IDS_STATUSBAR_CAPS_LOCK_ENABLED_PRESS_SEARCH;
  SetTooltipText(l10n_util::GetStringUTF16(id));
  SetAccessibleName(l10n_util::GetStringUTF16(id));
}

void CapsLockMenuButton::UpdateUIFromCurrentCapsLock(bool enabled) {
  SetVisible(enabled);
}

}  // namespace chromeos

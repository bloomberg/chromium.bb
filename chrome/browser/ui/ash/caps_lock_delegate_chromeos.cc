// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/caps_lock_delegate_chromeos.h"

#include "base/chromeos/chromeos_version.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/ime/xkeyboard.h"
#include "content/public/browser/browser_thread.h"

CapsLockDelegate::CapsLockDelegate(chromeos::input_method::XKeyboard* xkeyboard)
    : xkeyboard_(xkeyboard),
      is_running_on_chromeos_(base::chromeos::IsRunningOnChromeOS()),
      caps_lock_is_on_(xkeyboard_->CapsLockIsEnabled()) {
  chromeos::SystemKeyEventListener* system_event_listener =
      chromeos::SystemKeyEventListener::GetInstance();
  // SystemKeyEventListener should be instantiated when we're running production
  // code on Chrome OS.
  DCHECK(!is_running_on_chromeos_ || system_event_listener ||
         CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType));
  if (system_event_listener)
    system_event_listener->AddCapsLockObserver(this);
}

CapsLockDelegate::~CapsLockDelegate() {
  chromeos::SystemKeyEventListener* system_event_listener =
      chromeos::SystemKeyEventListener::GetInstance();
  if (system_event_listener)
    system_event_listener->RemoveCapsLockObserver(this);
}

bool CapsLockDelegate::IsCapsLockEnabled() const {
  return caps_lock_is_on_;
}

void CapsLockDelegate::SetCapsLockEnabled(bool enabled) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (is_running_on_chromeos_) {
    xkeyboard_->SetCapsLockEnabled(enabled);
    return;
  }
}

void CapsLockDelegate::ToggleCapsLock() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (is_running_on_chromeos_) {
    xkeyboard_->SetCapsLockEnabled(!caps_lock_is_on_);
    return;
  }
}

void CapsLockDelegate::OnCapsLockChange(bool enabled) {
  caps_lock_is_on_ = enabled;
}

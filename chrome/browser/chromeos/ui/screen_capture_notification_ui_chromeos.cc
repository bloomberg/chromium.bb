// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ui/screen_capture_notification_ui_chromeos.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

ScreenCaptureNotificationUIChromeOS::ScreenCaptureNotificationUIChromeOS(
    const base::string16& text)
    : text_(text) {
}

ScreenCaptureNotificationUIChromeOS::~ScreenCaptureNotificationUIChromeOS() {
  // MediaStreamCaptureIndicator will delete ScreenCaptureNotificationUI object
  // after it stops screen capture.
  ash::Shell::GetInstance()->system_tray_notifier()->NotifyScreenCaptureStop();
}

gfx::NativeViewId ScreenCaptureNotificationUIChromeOS::OnStarted(
    const base::Closure& stop_callback) {
  ash::Shell::GetInstance()->system_tray_notifier()->NotifyScreenCaptureStart(
      stop_callback, text_);
  return 0;
}

}  // namespace chromeos

// static
scoped_ptr<ScreenCaptureNotificationUI> ScreenCaptureNotificationUI::Create(
    const base::string16& text) {
  return scoped_ptr<ScreenCaptureNotificationUI>(
      new chromeos::ScreenCaptureNotificationUIChromeOS(text));
}

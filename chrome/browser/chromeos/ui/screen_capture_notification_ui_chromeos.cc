// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ui/screen_capture_notification_ui_chromeos.h"

#include "ash/system/tray/system_tray_notifier.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

ScreenCaptureNotificationUIChromeOS::ScreenCaptureNotificationUIChromeOS() {
}

ScreenCaptureNotificationUIChromeOS::~ScreenCaptureNotificationUIChromeOS() {
  // MediaStreamCaptureIndicator will delete ScreenCaptureNotificationUI object
  // after it stops screen capture.
  ash::Shell::GetInstance()->system_tray_notifier()->NotifyScreenCaptureStop();
}

bool ScreenCaptureNotificationUIChromeOS::Show(
    const base::Closure& stop_callback,
    const string16& title) {
  ash::Shell::GetInstance()->system_tray_notifier()->NotifyScreenCaptureStart(
      stop_callback,
      l10n_util::GetStringFUTF16(IDS_MEDIA_SCREEN_CAPTURE_NOTIFICATION_TEXT,
                                 title));
  return true;
}

}  // namespace chromeos

// static
scoped_ptr<ScreenCaptureNotificationUI> ScreenCaptureNotificationUI::Create() {
  return scoped_ptr<ScreenCaptureNotificationUI>(
      new chromeos::ScreenCaptureNotificationUIChromeOS());
}

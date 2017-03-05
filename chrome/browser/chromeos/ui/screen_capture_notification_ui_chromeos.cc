// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ui/screen_capture_notification_ui_chromeos.h"

#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/wm_shell.h"

namespace chromeos {

ScreenCaptureNotificationUIChromeOS::ScreenCaptureNotificationUIChromeOS(
    const base::string16& text)
    : text_(text) {
}

ScreenCaptureNotificationUIChromeOS::~ScreenCaptureNotificationUIChromeOS() {
  // MediaStreamCaptureIndicator will delete ScreenCaptureNotificationUI object
  // after it stops screen capture.
  ash::WmShell::Get()->system_tray_notifier()->NotifyScreenCaptureStop();
}

gfx::NativeViewId ScreenCaptureNotificationUIChromeOS::OnStarted(
    const base::Closure& stop_callback) {
  ash::WmShell::Get()->system_tray_notifier()->NotifyScreenCaptureStart(
      stop_callback, text_);
  return 0;
}

}  // namespace chromeos

// static
std::unique_ptr<ScreenCaptureNotificationUI>
ScreenCaptureNotificationUI::Create(const base::string16& text) {
  return std::unique_ptr<ScreenCaptureNotificationUI>(
      new chromeos::ScreenCaptureNotificationUIChromeOS(text));
}

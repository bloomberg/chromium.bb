// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_UI_SCREEN_CAPTURE_NOTIFICATION_UI_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_UI_SCREEN_CAPTURE_NOTIFICATION_UI_CHROMEOS_H_

#include "base/observer_list.h"
#include "chrome/browser/ui/screen_capture_notification_ui.h"

namespace chromeos {

// Chromeos implementation for ScreenCaptureNotificationUI.
class ScreenCaptureNotificationUIChromeOS : public ScreenCaptureNotificationUI {
 public:
  ScreenCaptureNotificationUIChromeOS();
  virtual ~ScreenCaptureNotificationUIChromeOS();

  // ScreenCaptureNotificationUI overrides.
  virtual bool Show(const base::Closure& stop_callback,
                    const string16& title) OVERRIDE;

  static scoped_ptr<ScreenCaptureNotificationUI> Create();

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenCaptureNotificationUIChromeOS);
};

}  // namespace chromeos
#endif  // CHROME_BROWSER_CHROMEOS_UI_SCREEN_CAPTURE_NOTIFICATION_UI_CHROMEOS_H_

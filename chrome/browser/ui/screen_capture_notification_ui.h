// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SCREEN_CAPTURE_NOTIFICATION_UI_H_
#define CHROME_BROWSER_UI_SCREEN_CAPTURE_NOTIFICATION_UI_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"

// Interface for screen capture notification UI shown when content of the screen
// is being captured.
class ScreenCaptureNotificationUI : public MediaStreamUI {
 public:
  ScreenCaptureNotificationUI() = default;
  ~ScreenCaptureNotificationUI() override = default;

  // Creates platform-specific screen capture notification UI. |text| specifies
  // the text that should be shown in the notification.
  static std::unique_ptr<ScreenCaptureNotificationUI> Create(
      const base::string16& text);

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenCaptureNotificationUI);
};

#endif  // CHROME_BROWSER_UI_SCREEN_CAPTURE_NOTIFICATION_UI_H_

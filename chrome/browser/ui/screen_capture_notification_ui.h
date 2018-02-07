// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SCREEN_CAPTURE_NOTIFICATION_UI_H_
#define CHROME_BROWSER_UI_SCREEN_CAPTURE_NOTIFICATION_UI_H_

#include "base/callback.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "content/public/common/media_stream_request.h"

// Interface for screen capture notification UI shown when content of the screen
// is being captured.
class ScreenCaptureNotificationUI : public content::MediaStreamUI {
 public:
  ~ScreenCaptureNotificationUI() override {}

  // Creates platform-specific screen capture notification UI. |text| specifies
  // the text that should be shown in the notification.
  static std::unique_ptr<ScreenCaptureNotificationUI> Create(
      const base::string16& text);
#if defined(OS_MACOSX)
  // Temporary shim for Polychrome. See bottom of first comment in
  // https://crbug.com/80495 for details.
  static std::unique_ptr<ScreenCaptureNotificationUI> CreateCocoa(
      const base::string16& text);
#endif
};

#endif  // CHROME_BROWSER_UI_SCREEN_CAPTURE_NOTIFICATION_UI_H_

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SCREEN_CAPTURE_NOTIFICATION_UI_H_
#define CHROME_BROWSER_UI_SCREEN_CAPTURE_NOTIFICATION_UI_H_

#include "base/callback.h"
#include "base/string16.h"

// Interface for screen capture notification UI shown when content of the screen
// is being captured.
class ScreenCaptureNotificationUI {
 public:
  virtual ~ScreenCaptureNotificationUI() {}

  // Shows the screen capture notification, allowing the user to stop it.
  // |stop_callback| will be invoked when the user chooses to stop capturing.
  // |title| specifies the title of the application that's capturing the screen.
  // Returns false if the window cannot be shown for any reason (|stop_callback|
  // is ignored in that case).
  virtual bool Show(const base::Closure& stop_callback,
                    const string16& title) = 0;

  // Creates platform-specific screen capture notification UI.
  static scoped_ptr<ScreenCaptureNotificationUI> Create();
};

#endif  // CHROME_BROWSER_UI_SCREEN_CAPTURE_NOTIFICATION_UI_H_

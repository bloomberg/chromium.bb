// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/screen_capture_notification_ui.h"

#include "base/logging.h"

// Stub implementation of the ScreenCaptureNotificationUI interface.
class ScreenCaptureNotificationUIStub : public ScreenCaptureNotificationUI {
 public:
  ScreenCaptureNotificationUIStub() {}
  virtual ~ScreenCaptureNotificationUIStub() {}

  virtual void OnStarted(const base::Closure& stop_callback) OVERRIDE {
    NOTIMPLEMENTED();
  }
};

// static
scoped_ptr<ScreenCaptureNotificationUI> ScreenCaptureNotificationUI::Create(
    const string16& title) {
  return scoped_ptr<ScreenCaptureNotificationUI>(
      new ScreenCaptureNotificationUIStub());
}

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/test_panel_notification_observer.h"

#include "chrome/browser/chrome_notification_types.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

TestPanelNotificationObserver::TestPanelNotificationObserver(
    int notification,
    const content::NotificationSource& source)
    : seen_(false),
      running_(false) {
  registrar_.Add(this, notification, source);
}

TestPanelNotificationObserver::~TestPanelNotificationObserver() {}

void TestPanelNotificationObserver::Wait() {
  if (seen_ || AtExpectedState())
    return;

  running_ = true;
  message_loop_runner_ = new content::MessageLoopRunner;
  message_loop_runner_->Run();
  EXPECT_TRUE(seen_);
}

void TestPanelNotificationObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (!running_)
    return;

  if (AtExpectedState()) {
    seen_ = true;
    message_loop_runner_->Quit();
    running_ = false;
  }
}

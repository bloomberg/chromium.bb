// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_TEST_PANEL_NOTIFICATION_OBSERVER_H_
#define CHROME_BROWSER_UI_PANELS_TEST_PANEL_NOTIFICATION_OBSERVER_H_

#include "base/memory/ref_counted.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace content {
class MessageLoopRunner;
class NotificationSource;
}

// Common base class for a custom notification observer that waits
// on a notification until an expected state is achieved.
// Modeled after ui_test_utils notification observers, but can handle
// more than one occurrence of the notification.
class TestPanelNotificationObserver : public content::NotificationObserver {
 public:
  TestPanelNotificationObserver(int notification,
                                const content::NotificationSource& source);
  virtual ~TestPanelNotificationObserver();

  // Wait until the expected state is achieved.
  void Wait();

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 protected:
  virtual bool AtExpectedState() = 0;

  bool seen_;  // true after transition to expected state has been seen
  bool running_;  // indicates whether message loop is running
  content::NotificationRegistrar registrar_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(TestPanelNotificationObserver);
};

#endif  // CHROME_BROWSER_UI_PANELS_TEST_PANEL_NOTIFICATION_OBSERVER_H_

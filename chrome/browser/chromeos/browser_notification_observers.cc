// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/browser_notification_observers.h"

#include "chrome/browser/chrome_thread.h"
#include "chrome/common/notification_service.h"

namespace {

// Static function that records uptime in /proc/uptime to tmp for metrics use.
void RecordUptime() {
  system("cat /proc/uptime > /tmp/uptime-chrome-first-render &");
}

}  // namespace

namespace chromeos {

InitialTabNotificationObserver::InitialTabNotificationObserver() {
  registrar_.Add(this, NotificationType::LOAD_START,
                 NotificationService::AllSources());
}

InitialTabNotificationObserver::~InitialTabNotificationObserver() {
}

void InitialTabNotificationObserver::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  // Only log for first tab to render.  Make sure this is only done once.
  if (type == NotificationType::LOAD_START &&
      num_tabs_.GetNext() == 0) {
    // If we can't post it, it doesn't matter.
    ChromeThread::PostTask(
        ChromeThread::FILE, FROM_HERE,
        NewRunnableFunction(RecordUptime));
    registrar_.Remove(this, NotificationType::LOAD_START,
                      NotificationService::AllSources());
  }
}

}  // namespace chromeos

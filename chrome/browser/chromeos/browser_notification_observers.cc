// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/browser_notification_observers.h"

#include <string>

#include "base/file_util.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/login/authentication_notification_details.h"
#include "chrome/common/notification_service.h"

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
  if (type == NotificationType::LOAD_START && num_tabs_.GetNext() == 0) {
    // If we can't post it, it doesn't matter.
    BootTimesLoader::RecordCurrentStats("chrome-first-render");
    registrar_.Remove(this, NotificationType::LOAD_START,
                      NotificationService::AllSources());
  }
}

LogLoginSuccessObserver::LogLoginSuccessObserver() {
  registrar_.Add(this, NotificationType::LOGIN_AUTHENTICATION,
                 NotificationService::AllSources());
}

LogLoginSuccessObserver::~LogLoginSuccessObserver() {
}

void LogLoginSuccessObserver::Observe(NotificationType type,
                                   const NotificationSource& source,
                                   const NotificationDetails& details) {
  DCHECK(type == NotificationType::LOGIN_AUTHENTICATION);
  Details<AuthenticationNotificationDetails> auth_details(details);
  if (auth_details->success()) {
    // If we can't post it, it doesn't matter.
    BootTimesLoader::RecordCurrentStats("login-successful");
    registrar_.Remove(this, NotificationType::LOGIN_AUTHENTICATION,
                      NotificationService::AllSources());
  }
}

}  // namespace chromeos

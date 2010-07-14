// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BROWSER_NOTIFICATION_OBSERVERS_H_
#define CHROME_BROWSER_CHROMEOS_BROWSER_NOTIFICATION_OBSERVERS_H_

#include "base/atomic_sequence_num.h"
#include "base/singleton.h"
#include "base/time.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"

// Global notification observers for chrome os.
namespace chromeos {

// Notification observer to log the initial time of when the first tab
// page is rendered for Chrome OS.
class InitialTabNotificationObserver : public NotificationObserver {
 public:
  InitialTabNotificationObserver();
  virtual ~InitialTabNotificationObserver();

  static InitialTabNotificationObserver* Get() {
    return Singleton<InitialTabNotificationObserver>::get();
  }

  void SetLoginSuccessTime() {
    login_success_time_ = base::Time::NowFromSystemTime();
  }

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  NotificationRegistrar registrar_;
  base::AtomicSequenceNumber num_tabs_;
  base::Time login_success_time_;

  DISALLOW_COPY_AND_ASSIGN(InitialTabNotificationObserver);
};

// Collects LOGIN_AUTHENTICATION notifications and logs uptime
// when login was successful.
class LogLoginSuccessObserver : public NotificationObserver {
 public:
   LogLoginSuccessObserver();
   virtual ~LogLoginSuccessObserver();

   static LogLoginSuccessObserver* Get() {
     return Singleton<LogLoginSuccessObserver>::get();
   }

  // NotificationObserver interface.
  virtual void Observe(NotificationType type, const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(LogLoginSuccessObserver);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_BROWSER_NOTIFICATION_OBSERVERS_H_

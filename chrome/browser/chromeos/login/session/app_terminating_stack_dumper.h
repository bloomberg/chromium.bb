// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SESSION_APP_TERMINATING_STACK_DUMPER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SESSION_APP_TERMINATING_STACK_DUMPER_H_

#include "base/macros.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace chromeos {

// Helper class that dumps stack trace on NOTIFICATION_APP_TERMINATING.
// TODO(crbug.com/717585): Remove after the root cause of bug identified.
class AppTerminatingStackDumper : public content::NotificationObserver {
 public:
  AppTerminatingStackDumper();
  ~AppTerminatingStackDumper() override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 private:
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(AppTerminatingStackDumper);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SESSION_APP_TERMINATING_STACK_DUMPER_H_

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ENTERPRISE_EXTENSION_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_ENTERPRISE_EXTENSION_OBSERVER_H_
#pragma once

#include "chrome/common/extensions/extension.h"
#include "content/common/notification_details.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_source.h"
#include "content/common/notification_type.h"

class FilePath;
class Profile;

namespace chromeos {

// This observer listens for installed extensions and restarts the ChromeOS
// Enterprise daemon if an Enterprise Extension gets installed.
class EnterpriseExtensionObserver
    : public NotificationObserver {
 public:
  explicit EnterpriseExtensionObserver(Profile* profile);
  virtual ~EnterpriseExtensionObserver() {}

  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details);

 private:
  static void CheckExtensionAndNotifyEntd(const FilePath& path);
  static void NotifyEntd();

  Profile* profile_;
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(EnterpriseExtensionObserver);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ENTERPRISE_EXTENSION_OBSERVER_H_

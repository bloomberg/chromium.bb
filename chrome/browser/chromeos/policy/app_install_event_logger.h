// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_APP_INSTALL_EVENT_LOGGER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_APP_INSTALL_EVENT_LOGGER_H_

#include <set>
#include <string>

#include "base/macros.h"

class Profile;

namespace enterprise_management {
class AppInstallReportLogEvent;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace policy {

// Listens for and logs events related to app push-installs.
class AppInstallEventLogger {
 public:
  // The delegate that events are forwarded to for inclusion in the log.
  class Delegate {
   public:
    // Adds an identical log entry for every app in |packages|.
    virtual void Add(
        const std::set<std::string>& packages,
        const enterprise_management::AppInstallReportLogEvent& event) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  // Delegate must outlive |this|.
  AppInstallEventLogger(Delegate* delegate, Profile* profile);
  ~AppInstallEventLogger() = default;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Clears all data related to app-install event log collection for |profile|.
  // Must not be called while an |AppInstallEventLogger| exists for |profile|.
  static void Clear(Profile* profile);

 private:
  DISALLOW_COPY_AND_ASSIGN(AppInstallEventLogger);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_APP_INSTALL_EVENT_LOGGER_H_

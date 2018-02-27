// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_APP_INSTALL_EVENT_LOG_COLLECTOR_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_APP_INSTALL_EVENT_LOG_COLLECTOR_H_

#include <memory>
#include <set>
#include <string>

#include "base/macros.h"

class Profile;

namespace enterprise_management {
class AppInstallReportLogEvent;
}
namespace policy {

// Listens for and logs events related to app push-installs.
class AppInstallEventLogCollector {
 public:
  // The delegate that events are forwarded to for inclusion in the log.
  class Delegate {
   public:
    // Adds an identical log entry for every app whose push-install is pending.
    // The |event|'s timestamp is set to the current time if not set yet.
    virtual void AddForAllPackages(
        std::unique_ptr<enterprise_management::AppInstallReportLogEvent>
            event) = 0;

    // Adds a log entry for |package|. The |event|'s timestamp is set to the
    // current time if not set yet.
    virtual void Add(
        const std::string& package,
        std::unique_ptr<enterprise_management::AppInstallReportLogEvent>
            event) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  // Delegate must outlive |this|.
  AppInstallEventLogCollector(Delegate* delegate,
                              Profile* profile,
                              const std::set<std::string>& pending_packages);
  ~AppInstallEventLogCollector();

  // Called whenever the list of pending app-install requests changes.
  void OnPendingPackagesChanged(const std::set<std::string>& added,
                                const std::set<std::string>& removed);

 private:
  DISALLOW_COPY_AND_ASSIGN(AppInstallEventLogCollector);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_APP_INSTALL_EVENT_LOG_COLLECTOR_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_EVENT_BASED_STATUS_REPORTING_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_EVENT_BASED_STATUS_REPORTING_SERVICE_H_

#include <string>

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chromeos/dbus/power_manager_client.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "net/base/network_change_notifier.h"

namespace content {
class BrowserContext;
}

namespace chromeos {

// Requests status report when events relevant to supervision features happen.
// The events that are triggers to status report are:
//     * App install
//     * App update
//     * Device lock
//     * Device unlock
//     * Device connected
//     * Device returns from suspend mode
class EventBasedStatusReportingService
    : public KeyedService,
      public ArcAppListPrefs::Observer,
      public session_manager::SessionManagerObserver,
      public net::NetworkChangeNotifier::NetworkChangeObserver,
      public PowerManagerClient::Observer {
 public:
  explicit EventBasedStatusReportingService(content::BrowserContext* context);
  ~EventBasedStatusReportingService() override;

  // ArcAppListPrefs::Observer:
  void OnPackageInstalled(
      const arc::mojom::ArcPackageInfo& package_info) override;
  void OnPackageModified(
      const arc::mojom::ArcPackageInfo& package_info) override;

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

  // net::NetworkChangeNotifier::NetworkChangeObserver:
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  // PowerManagerClient::Observer:
  void SuspendDone(const base::TimeDelta& duration) override;

 private:
  void RequestStatusReport(const std::string& reason);

  // KeyedService:
  void Shutdown() override;

  content::BrowserContext* const context_;
  bool session_just_started_ = true;

  DISALLOW_COPY_AND_ASSIGN(EventBasedStatusReportingService);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_EVENT_BASED_STATUS_REPORTING_SERVICE_H_

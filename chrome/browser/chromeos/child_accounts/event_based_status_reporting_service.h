// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_EVENT_BASED_STATUS_REPORTING_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_EVENT_BASED_STATUS_REPORTING_SERVICE_H_

#include "base/macros.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}

namespace chromeos {

// Requests status report when events relevant to supervision features happen.
class EventBasedStatusReportingService : public KeyedService,
                                         public ArcAppListPrefs::Observer {
 public:
  explicit EventBasedStatusReportingService(content::BrowserContext* context);
  ~EventBasedStatusReportingService() override;

  // ArcAppListPrefs::Observer:
  void OnPackageInstalled(
      const arc::mojom::ArcPackageInfo& package_info) override;
  void OnPackageModified(
      const arc::mojom::ArcPackageInfo& package_info) override;

 private:
  // KeyedService:
  void Shutdown() override;

  content::BrowserContext* const context_;

  DISALLOW_COPY_AND_ASSIGN(EventBasedStatusReportingService);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_EVENT_BASED_STATUS_REPORTING_SERVICE_H_

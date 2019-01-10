// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/event_based_status_reporting_service.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/child_accounts/consumer_status_reporting_service.h"
#include "chrome/browser/chromeos/child_accounts/consumer_status_reporting_service_factory.h"
#include "chrome/browser/profiles/profile.h"

namespace chromeos {

EventBasedStatusReportingService::EventBasedStatusReportingService(
    content::BrowserContext* context)
    : context_(context) {
  ArcAppListPrefs* arc_app_prefs = ArcAppListPrefs::Get(context_);
  // arc_app_prefs may not available in some browser tests.
  if (!arc_app_prefs)
    return;
  arc_app_prefs->AddObserver(this);
}

EventBasedStatusReportingService::~EventBasedStatusReportingService() = default;

void EventBasedStatusReportingService::OnPackageInstalled(
    const arc::mojom::ArcPackageInfo& package_info) {
  VLOG(1) << "Request status report due to an app install.";
  ConsumerStatusReportingServiceFactory::GetForBrowserContext(context_)
      ->RequestImmediateStatusReport();
}

void EventBasedStatusReportingService::OnPackageModified(
    const arc::mojom::ArcPackageInfo& package_info) {
  VLOG(1) << "Request status report due to an app update.";
  ConsumerStatusReportingServiceFactory::GetForBrowserContext(context_)
      ->RequestImmediateStatusReport();
}

void EventBasedStatusReportingService::Shutdown() {
  ArcAppListPrefs* arc_app_prefs = ArcAppListPrefs::Get(context_);
  // arc_app_prefs may not available in some browser tests.
  if (!arc_app_prefs)
    return;
  arc_app_prefs->RemoveObserver(this);
}

}  // namespace chromeos

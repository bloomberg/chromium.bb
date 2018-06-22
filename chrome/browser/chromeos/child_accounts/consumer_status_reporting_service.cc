// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/consumer_status_reporting_service.h"

#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/policy/device_status_collector.h"
#include "chrome/browser/chromeos/policy/status_uploader.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/user_policy_manager_factory_chromeos.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/system/statistics_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"

namespace chromeos {

namespace {

// Default frequency for uploading status reports.
constexpr base::TimeDelta kStatusUploadFrequency =
    base::TimeDelta::FromMinutes(10);

}  // namespace

ConsumerStatusReportingService::ConsumerStatusReportingService(
    content::BrowserContext* context) {
  // If child user is registered with DMServer and has User Policy applied, it
  // should upload device status to the server.
  policy::UserCloudPolicyManagerChromeOS* user_cloud_policy_manager =
      policy::UserPolicyManagerFactoryChromeOS::GetCloudPolicyManagerForProfile(
          Profile::FromBrowserContext(context));
  if (!user_cloud_policy_manager) {
    LOG(WARNING) << "Child user is not managed by User Policy - status reports "
                    "cannot be uploaded to the server. ";
    return;
  }

  PrefService* pref_service = Profile::FromBrowserContext(context)->GetPrefs();
  DCHECK(pref_service->GetInitializationStatus() !=
         PrefService::INITIALIZATION_STATUS_WAITING);

  status_uploader_ = std::make_unique<policy::StatusUploader>(
      user_cloud_policy_manager->core()->client(),
      std::make_unique<policy::DeviceStatusCollector>(
          pref_service, system::StatisticsProvider::GetInstance(),
          policy::DeviceStatusCollector::VolumeInfoFetcher(),
          policy::DeviceStatusCollector::CPUStatisticsFetcher(),
          policy::DeviceStatusCollector::CPUTempFetcher(),
          policy::DeviceStatusCollector::AndroidStatusFetcher(),
          false /* is_enterprise_reporting */),
      base::ThreadTaskRunnerHandle::Get(), kStatusUploadFrequency);
}

ConsumerStatusReportingService::~ConsumerStatusReportingService() = default;

}  // namespace chromeos

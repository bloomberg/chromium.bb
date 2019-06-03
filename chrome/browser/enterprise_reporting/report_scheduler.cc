// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise_reporting/report_scheduler.h"

#include <utility>
#include <vector>

#include "base/task/post_task.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise_reporting/prefs.h"
#include "chrome/browser/enterprise_reporting/request_timer.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/browser_dm_token_storage.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace em = enterprise_management;

namespace enterprise_reporting {
namespace {
const int kDefaultUploadIntervalHours =
    24;  // Default upload interval is 24 hours.

// Reads DM token and client id. Returns true if boths are non empty.
bool GetDMTokenAndDeviceId(std::string* dm_token, std::string* client_id) {
  DCHECK(dm_token && client_id);
  *dm_token = policy::BrowserDMTokenStorage::Get()->RetrieveDMToken();
  *client_id = policy::BrowserDMTokenStorage::Get()->RetrieveClientId();

  if (dm_token->empty() || client_id->empty()) {
    VLOG(1)
        << "Enterprise reporting is disabled because device is not enrolled.";
    return false;
  }
  return true;
}

// Returns true if cloud reporting is enabled.
bool IsReportingEnabled() {
  return g_browser_process->local_state()->GetBoolean(
      prefs::kCloudReportingEnabled);
}

}  // namespace

ReportScheduler::ReportScheduler(
    std::unique_ptr<policy::CloudPolicyClient> client,
    std::unique_ptr<RequestTimer> request_timer)
    : cloud_policy_client_(std::move(client)),
      request_timer_(std::move(request_timer)) {
  RegisterPerfObserver();
}

ReportScheduler::~ReportScheduler() = default;

void ReportScheduler::RegisterPerfObserver() {
  pref_change_registrar_.Init(g_browser_process->local_state());
  pref_change_registrar_.Add(
      prefs::kCloudReportingEnabled,
      base::BindRepeating(&ReportScheduler::OnReportEnabledPerfChanged,
                          base::Unretained(this)));
  // Trigger first perf check during launch process.
  OnReportEnabledPerfChanged();
}

void ReportScheduler::OnReportEnabledPerfChanged() {
  std::string dm_token;
  std::string client_id;
  if (!IsReportingEnabled() || !GetDMTokenAndDeviceId(&dm_token, &client_id)) {
    if (request_timer_)
      request_timer_->Stop();
    return;
  }

  if (!cloud_policy_client_->is_registered()) {
    cloud_policy_client_->SetupRegistration(dm_token, client_id,
                                            std::vector<std::string>());
  }

  Start();
}

void ReportScheduler::Start() {
  base::TimeDelta upload_interval =
      base::TimeDelta::FromHours(kDefaultUploadIntervalHours);
  base::TimeDelta first_request_delay =
      upload_interval -
      (base::Time::Now() -
       g_browser_process->local_state()->GetTime(kLastUploadTimestamp));
  // The first report delay is based on the |lastUploadTimestamp| in the
  // |local_state|, after that, it's 24 hours for each succeeded upload.
  request_timer_->Start(
      FROM_HERE, first_request_delay, upload_interval,
      base::BindRepeating(&ReportScheduler::GenerateAndUploadReport,
                          base::Unretained(this)));
}

void ReportScheduler::GenerateAndUploadReport() {
  VLOG(1) << "Uploading enterprise report.";
  // TODO(zmin): Generates a real request.
  std::unique_ptr<em::ChromeDesktopReportRequest> request =
      std::make_unique<em::ChromeDesktopReportRequest>();
  cloud_policy_client_->UploadChromeDesktopReport(
      std::move(request),
      base::BindRepeating(&ReportScheduler::OnReportUploaded,
                          base::Unretained(this)));
}

void ReportScheduler::OnReportUploaded(bool status) {
  if (status) {
    VLOG(1) << "The enterprise report has been uploaded.";
    g_browser_process->local_state()->SetTime(kLastUploadTimestamp,
                                              base::Time::Now());
    if (IsReportingEnabled())
      request_timer_->Reset();
    return;
  }
  VLOG(1) << "The enterprise report has not been uploaded.";
  // TODO(zmin): Implement retry logic
}

}  // namespace enterprise_reporting

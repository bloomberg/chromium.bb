// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/auth/arc_active_directory_enrollment_token_fetcher.h"

#include <string>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/dm_token_storage.h"
#include "chrome/browser/chromeos/settings/install_attributes.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "net/url_request/url_request_context_getter.h"

namespace {

policy::DeviceManagementService* GetDeviceManagementService() {
  policy::BrowserPolicyConnectorChromeOS* const connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return connector->device_management_service();
}

std::string GetClientId() {
  policy::BrowserPolicyConnectorChromeOS* const connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return connector->GetInstallAttributes()->GetDeviceId();
}

}  // namespace

namespace arc {

ArcActiveDirectoryEnrollmentTokenFetcher::
    ArcActiveDirectoryEnrollmentTokenFetcher()
    : weak_ptr_factory_(this) {}

ArcActiveDirectoryEnrollmentTokenFetcher::
    ~ArcActiveDirectoryEnrollmentTokenFetcher() = default;

void ArcActiveDirectoryEnrollmentTokenFetcher::Fetch(
    const FetchCallback& callback) {
  DCHECK(callback_.is_null());
  callback_ = callback;
  dm_token_storage_ = base::MakeUnique<policy::DMTokenStorage>(
      g_browser_process->local_state());
  dm_token_storage_->RetrieveDMToken(
      base::Bind(&ArcActiveDirectoryEnrollmentTokenFetcher::OnDMTokenAvailable,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ArcActiveDirectoryEnrollmentTokenFetcher::OnDMTokenAvailable(
    const std::string& dm_token) {
  DCHECK(!fetch_request_job_);

  if (dm_token.empty()) {
    LOG(ERROR) << "Retrieving the DMToken failed.";
    base::ResetAndReturn(&callback_)
        .Run(Status::FAILURE, std::string(), std::string());
    return;
  }

  policy::DeviceManagementService* service = GetDeviceManagementService();
  fetch_request_job_.reset(
      service->CreateJob(policy::DeviceManagementRequestJob::
                             TYPE_ACTIVE_DIRECTORY_ENROLL_PLAY_USER,
                         g_browser_process->system_request_context()));

  fetch_request_job_->SetDMToken(dm_token);
  fetch_request_job_->SetClientID(GetClientId());
  fetch_request_job_->GetRequest()
      ->mutable_active_directory_enroll_play_user_request();

  fetch_request_job_->Start(
      base::Bind(&ArcActiveDirectoryEnrollmentTokenFetcher::
                     OnFetchEnrollmentTokenCompleted,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ArcActiveDirectoryEnrollmentTokenFetcher::OnFetchEnrollmentTokenCompleted(
    policy::DeviceManagementStatus dm_status,
    int net_error,
    const enterprise_management::DeviceManagementResponse& response) {
  fetch_request_job_.reset();

  Status fetch_status;
  std::string enrollment_token;
  std::string user_id;

  switch (dm_status) {
    case policy::DM_STATUS_SUCCESS:
      if (!response.has_active_directory_enroll_play_user_response()) {
        LOG(WARNING) << "Invalid Active Directory enroll Play user response.";
        fetch_status = Status::FAILURE;
        break;
      }
      fetch_status = Status::SUCCESS;
      enrollment_token = response.active_directory_enroll_play_user_response()
                             .enrollment_token();
      user_id = response.active_directory_enroll_play_user_response().user_id();
      break;
    case policy::DM_STATUS_SERVICE_ARC_DISABLED:
      fetch_status = Status::ARC_DISABLED;
      break;
    default:  // All other error cases
      LOG(ERROR) << "Fetching an enrollment token failed. DM Status: "
                 << dm_status;
      fetch_status = Status::FAILURE;
      break;
  }

  base::ResetAndReturn(&callback_).Run(fetch_status, enrollment_token, user_id);
}

}  // namespace arc

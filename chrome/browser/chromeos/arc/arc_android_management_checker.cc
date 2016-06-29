// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_android_management_checker.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/arc/arc_android_management_checker_delegate.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"

namespace arc {

namespace {

constexpr int kRetryTimeMinMs = 10 * 1000;           // 10 sec.
constexpr int kRetryTimeMaxMs = 1 * 60 * 60 * 1000;  // 1 hour.

policy::DeviceManagementService* GetDeviceManagementService() {
  policy::BrowserPolicyConnectorChromeOS* const connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return connector->device_management_service();
}

}  // namespace

ArcAndroidManagementChecker::ArcAndroidManagementChecker(
    ArcAndroidManagementCheckerDelegate* delegate,
    ProfileOAuth2TokenService* token_service,
    const std::string& account_id,
    bool background_mode)
    : delegate_(delegate),
      token_service_(token_service),
      account_id_(account_id),
      background_mode_(background_mode),
      retry_time_ms_(kRetryTimeMinMs),
      android_management_client_(GetDeviceManagementService(),
                                 g_browser_process->system_request_context(),
                                 account_id,
                                 token_service),
      weak_ptr_factory_(this) {
  if (token_service_->RefreshTokenIsAvailable(account_id_)) {
    StartCheck();
  } else {
    DCHECK(background_mode_);
    token_service_->AddObserver(this);
  }
}

ArcAndroidManagementChecker::~ArcAndroidManagementChecker() {
  token_service_->RemoveObserver(this);
}

// static
void ArcAndroidManagementChecker::StartClient() {
  GetDeviceManagementService()->ScheduleInitialization(0);
}

void ArcAndroidManagementChecker::OnRefreshTokenAvailable(
    const std::string& account_id) {
  if (account_id != account_id_)
    return;
  OnRefreshTokensLoaded();
}

void ArcAndroidManagementChecker::OnRefreshTokensLoaded() {
  token_service_->RemoveObserver(this);
  StartCheck();
}

void ArcAndroidManagementChecker::StartCheck() {
  if (!token_service_->RefreshTokenIsAvailable(account_id_)) {
    VLOG(2) << "No refresh token is available for android management check.";
    OnAndroidManagementChecked(
        policy::AndroidManagementClient::Result::RESULT_ERROR);
    return;
  }

  VLOG(2) << "Start android management check.";
  android_management_client_.StartCheckAndroidManagement(
      base::Bind(&ArcAndroidManagementChecker::OnAndroidManagementChecked,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ArcAndroidManagementChecker::ScheduleCheck() {
  DCHECK(background_mode_);

  VLOG(2) << "Schedule next android management  check in " << retry_time_ms_
          << " ms.";

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&ArcAndroidManagementChecker::StartCheck,
                            weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(retry_time_ms_));
  retry_time_ms_ *= 2;
  if (retry_time_ms_ > kRetryTimeMaxMs)
    retry_time_ms_ = kRetryTimeMaxMs;
}

void ArcAndroidManagementChecker::DispatchResult(
    policy::AndroidManagementClient::Result result) {
  DCHECK(delegate_);
  delegate_->OnAndroidManagementChecked(result);
}

void ArcAndroidManagementChecker::OnAndroidManagementChecked(
    policy::AndroidManagementClient::Result result) {
  VLOG(2) << "Android management check done " << result << ".";
  if (background_mode_ &&
      result == policy::AndroidManagementClient::Result::RESULT_ERROR) {
    ScheduleCheck();
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&ArcAndroidManagementChecker::DispatchResult,
                            weak_ptr_factory_.GetWeakPtr(), result));
}

}  // namespace arc

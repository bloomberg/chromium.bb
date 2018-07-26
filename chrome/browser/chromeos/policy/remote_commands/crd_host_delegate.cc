// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/remote_commands/crd_host_delegate.h"

#include "components/user_manager/user_manager.h"
#include "ui/base/user_activity/user_activity_detector.h"

namespace policy {

CRDHostDelegate::CRDHostDelegate() {}

CRDHostDelegate::~CRDHostDelegate() {
  // TODO(antrim): shutdown host somewhat correctly.
}

bool CRDHostDelegate::HasActiveSession() {
  return false;
}

void CRDHostDelegate::TerminateSession(base::OnceClosure callback) {
  std::move(callback).Run();
}

bool CRDHostDelegate::AreServicesReady() {
  return user_manager::UserManager::IsInitialized() &&
         ui::UserActivityDetector::Get() != nullptr;
}

bool CRDHostDelegate::IsRunningKiosk() {
  auto* user_manager = user_manager::UserManager::Get();
  // TODO(antrim): find out if Arc Kiosk is also eligible.
  // TODO(antrim): find out if only auto-started Kiosks are elidgible.
  return user_manager->IsLoggedInAsKioskApp();
}

base::TimeDelta CRDHostDelegate::GetIdlenessPeriod() {
  return base::TimeTicks::Now() -
         ui::UserActivityDetector::Get()->last_activity_time();
}

void CRDHostDelegate::FetchOAuthToken(
    DeviceCommandStartCRDSessionJob::OAuthTokenCallback success_callback,
    DeviceCommandStartCRDSessionJob::ErrorCallback error_callback) {
  // TODO(antrim): implement
  std::move(success_callback).Run(std::string("OAuth token"));
}

void CRDHostDelegate::FetchICEConfig(
    const std::string& oauth_token,
    DeviceCommandStartCRDSessionJob::ICEConfigCallback success_callback,
    DeviceCommandStartCRDSessionJob::ErrorCallback error_callback) {
  base::Value configuration(base::Value::Type::DICTIONARY);
  // TODO(antrim): implement
  std::move(success_callback).Run(std::move(configuration));
}

void CRDHostDelegate::StartCRDHostAndGetCode(
    const std::string& directory_bot_jid,
    const std::string& oauth_token,
    base::Value ice_config,
    DeviceCommandStartCRDSessionJob::AuthCodeCallback success_callback,
    DeviceCommandStartCRDSessionJob::ErrorCallback error_callback) {
  // TODO(antrim): implement
  std::move(success_callback).Run(std::string("Auth Code"));
}

}  // namespace policy

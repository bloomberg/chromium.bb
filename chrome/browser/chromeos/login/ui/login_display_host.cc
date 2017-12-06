// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/login_display_host.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/mobile_config.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"

namespace chromeos {
namespace {

// The delay of triggering initialization of the device policy subsystem
// after the login screen is initialized. This makes sure that device policy
// network requests are made while the system is idle waiting for user input.
constexpr int64_t kPolicyServiceInitializationDelayMilliseconds = 100;

}  // namespace

// static
LoginDisplayHost* LoginDisplayHost::default_host_ = nullptr;

LoginDisplayHost::LoginDisplayHost() : weak_factory_(this) {}

LoginDisplayHost::~LoginDisplayHost() = default;

void LoginDisplayHost::StartSignInScreen(const LoginScreenContext& context) {
  PrewarmAuthentication();

  const user_manager::UserList& users =
      user_manager::UserManager::Get()->GetUsers();

  // Fix for users who updated device and thus never passed register screen.
  // If we already have users, we assume that it is not a second part of
  // OOBE. See http://crosbug.com/6289
  if (!StartupUtils::IsDeviceRegistered() && !users.empty()) {
    VLOG(1) << "Mark device registered because there are remembered users: "
            << users.size();
    StartupUtils::MarkDeviceRegistered(base::OnceClosure());
  }

  // Initiate mobile config load.
  MobileConfig::GetInstance();

  // Initiate device policy fetching.
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  connector->ScheduleServiceInitialization(
      kPolicyServiceInitializationDelayMilliseconds);

  // Run UI-specific logic.
  OnStartSignInScreen(context);

  // Enable status area after starting sign-in screen, as it may depend on the
  // UI being visible.
  SetStatusAreaVisible(true);
}

void LoginDisplayHost::PrewarmAuthentication() {
  auth_prewarmer_ = std::make_unique<AuthPrewarmer>();
  auth_prewarmer_->PrewarmAuthentication(base::BindOnce(
      &LoginDisplayHost::OnAuthPrewarmDone, weak_factory_.GetWeakPtr()));
}

void LoginDisplayHost::OnAuthPrewarmDone() {
  auth_prewarmer_.reset();
}

}  // namespace chromeos

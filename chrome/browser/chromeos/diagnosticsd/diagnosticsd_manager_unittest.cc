// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/diagnosticsd/diagnosticsd_manager.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chromeos/dbus/upstart/fake_upstart_client.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

// An implementation of Upstart Client that tracks StartWilcoDtcService() /
// StopWilcoDtcService() calls.
class TestUpstartClient final : public FakeUpstartClient {
 public:
  // FakeUpstartClient overrides:
  void StartWilcoDtcService(
      chromeos::VoidDBusMethodCallback callback) override {
    is_wilco_dtc_started_ = true;
    EXPECT_FALSE(is_callback_called_);
    is_callback_called_ = true;
  }

  void StopWilcoDtcService(chromeos::VoidDBusMethodCallback callback) override {
    is_wilco_dtc_started_ = false;
    EXPECT_FALSE(is_callback_called_);
    is_callback_called_ = true;
  }

  // Returns true if wilco DTC support services are started.
  // Also resets the internal state, allowing new StartWilcoDtciService() /
  // StopWilcoDtcService() calls.
  bool GetAndResetWilcoDtcStarted() {
    EXPECT_TRUE(is_callback_called_);
    is_callback_called_ = false;
    return is_wilco_dtc_started_;
  }

 private:
  bool is_wilco_dtc_started_ = false;

  // Identifies whether StartWilcoDtcService() / StopWilcoDtcService() were
  // called since the last GetAndResetWilcoDtcStarted() obtained the state of
  // wilco DTC.
  bool is_callback_called_ = false;
};

// Tests DiagnosticsdManager class instance.
class DiagnosticsdManagerTest : public testing::Test {
 protected:
  DiagnosticsdManagerTest() {
    upstart_client_ = std::make_unique<TestUpstartClient>();
  }

  ~DiagnosticsdManagerTest() override {}

  void SetWilcoDtcAllowedPolicy(bool wilco_dtc_allowed) {
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        kDeviceWilcoDtcAllowed, wilco_dtc_allowed);
  }

  void LogInUser(bool is_affiliated) {
    AccountId account_id =
        AccountId::FromUserEmail(user_manager::kStubUserEmail);
    fake_user_manager_->AddUserWithAffiliation(account_id, is_affiliated);
    fake_user_manager_->LoginUser(account_id);
    session_manager_.SetSessionState(session_manager::SessionState::ACTIVE);
  }

  bool is_wilco_dtc_started() {
    return upstart_client_->GetAndResetWilcoDtcStarted();
  }

 private:
  base::MessageLoop message_loop_;
  ScopedTestingCrosSettings scoped_testing_cros_settings_;
  std::unique_ptr<TestUpstartClient> upstart_client_;
  FakeChromeUserManager* fake_user_manager_{new FakeChromeUserManager()};
  user_manager::ScopedUserManager scoped_user_manager_{
      base::WrapUnique(fake_user_manager_)};
  session_manager::SessionManager session_manager_;
};

// Test that wilco DTC support services are not started on enterprise enrolled
// devices with a certain device policy unset.
TEST_F(DiagnosticsdManagerTest, EnterpriseiWilcoDtcBasic) {
  DiagnosticsdManager diagnosticsd_manager;
  EXPECT_FALSE(is_wilco_dtc_started());
}

// Test that wilco DTC support services are not started if disabled by device
// policy.
TEST_F(DiagnosticsdManagerTest, EnterpriseWilcoDtcDisabled) {
  DiagnosticsdManager diagnosticsd_manager;
  EXPECT_FALSE(is_wilco_dtc_started());

  SetWilcoDtcAllowedPolicy(false);
  EXPECT_FALSE(is_wilco_dtc_started());
}

// Test that wilco DTC support services are started if enabled by policy.
TEST_F(DiagnosticsdManagerTest, EnterpriseWilcoDtcAllowed) {
  SetWilcoDtcAllowedPolicy(true);
  DiagnosticsdManager diagosticsd_manager;
  EXPECT_TRUE(is_wilco_dtc_started());
}

// Test that wilco DTC support services are not started if non-affiliated user
// is logged-in.
TEST_F(DiagnosticsdManagerTest, EnterpriseNonAffiliatedUserLoggedIn) {
  DiagnosticsdManager diagosticsd_manager;
  EXPECT_FALSE(is_wilco_dtc_started());

  SetWilcoDtcAllowedPolicy(true);
  EXPECT_TRUE(is_wilco_dtc_started());

  LogInUser(false);
  EXPECT_FALSE(is_wilco_dtc_started());
}

// Test that wilco DTC support services are started if enabled by device policy
// and affiliated user is logged-in.
TEST_F(DiagnosticsdManagerTest, EnterpriseAffiliatedUserLoggedIn) {
  SetWilcoDtcAllowedPolicy(true);
  DiagnosticsdManager diagosticsd_manager;
  EXPECT_TRUE(is_wilco_dtc_started());

  LogInUser(true);
  EXPECT_TRUE(is_wilco_dtc_started());
}

// Test that wilco DTC support services are not started if non-affiliated user
// is logged-in before the construction.
TEST_F(DiagnosticsdManagerTest, EnterpriseNonAffiliatedUserLoggedInBefore) {
  SetWilcoDtcAllowedPolicy(true);
  LogInUser(false);
  DiagnosticsdManager diagnosticsd_manager;

  EXPECT_FALSE(is_wilco_dtc_started());
}

}  // namespace

}  // namespace chromeos

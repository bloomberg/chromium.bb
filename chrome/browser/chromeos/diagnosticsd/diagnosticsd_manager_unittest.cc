// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/diagnosticsd/diagnosticsd_manager.h"

#include "base/barrier_closure.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/diagnosticsd/testing_diagnosticsd_bridge_wrapper.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/services/diagnosticsd/public/mojom/diagnosticsd.mojom.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/upstart/fake_upstart_client.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Invoke;
using testing::StrictMock;

namespace chromeos {

namespace {

// An implementation of Upstart Client that fakes a start/ stop of wilco DTC
// services on StartWilcoDtcService() / StopWilcoDtcService() calls.
class TestUpstartClient final : public FakeUpstartClient {
 public:
  // FakeUpstartClient overrides:
  void StartWilcoDtcService(
      chromeos::VoidDBusMethodCallback callback) override {
    std::move(callback).Run(true /* success */);
  }

  void StopWilcoDtcService(chromeos::VoidDBusMethodCallback callback) override {
    std::move(callback).Run(true /* success */);
  }
};

class MockMojoDiagnosticsdService
    : public diagnosticsd::mojom::DiagnosticsdService {
 public:
  MOCK_METHOD2(SendUiMessageToDiagnosticsProcessor,
               void(mojo::ScopedHandle,
                    SendUiMessageToDiagnosticsProcessorCallback));

  MOCK_METHOD0(NotifyConfigurationDataChanged, void());
};

// An implementation of the DiagnosticsdManager::Delegate that owns the testing
// instance of the DiagnosticsdBridge.
class FakeDiagnosticsdManagerDelegate final
    : public DiagnosticsdManager::Delegate {
 public:
  FakeDiagnosticsdManagerDelegate(
      MockMojoDiagnosticsdService* mojo_diagnosticsd_service)
      : mojo_diagnosticsd_service_(mojo_diagnosticsd_service) {}

  // DiagnosticsdManager::Delegate overrides:
  std::unique_ptr<DiagnosticsdBridge> CreateDiagnosticsdBridge() override {
    std::unique_ptr<DiagnosticsdBridge> diagnosticsd_bridge;
    testing_diagnosticsd_bridge_wrapper_ =
        TestingDiagnosticsdBridgeWrapper::Create(
            mojo_diagnosticsd_service_,
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_),
            &diagnosticsd_bridge);
    DCHECK(diagnosticsd_bridge);
    testing_diagnosticsd_bridge_wrapper_->EstablishFakeMojoConnection();
    return diagnosticsd_bridge;
  }

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<TestingDiagnosticsdBridgeWrapper>
      testing_diagnosticsd_bridge_wrapper_;
  MockMojoDiagnosticsdService* mojo_diagnosticsd_service_;
};

// Tests DiagnosticsdManager class instance.
class DiagnosticsdManagerTest : public testing::Test {
 protected:
  DiagnosticsdManagerTest() {
    DBusThreadManager::Initialize();
    upstart_client_ = std::make_unique<TestUpstartClient>();
  }

  ~DiagnosticsdManagerTest() override { DBusThreadManager::Shutdown(); }

  std::unique_ptr<DiagnosticsdManager::Delegate> CreateDelegate() {
    return std::make_unique<FakeDiagnosticsdManagerDelegate>(
        &mojo_diagnosticsd_service_);
  }

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

  MockMojoDiagnosticsdService* mojo_diagnosticsd_service() {
    return &mojo_diagnosticsd_service_;
  }

 private:
  base::MessageLoop message_loop_;
  ScopedTestingCrosSettings scoped_testing_cros_settings_;
  std::unique_ptr<TestUpstartClient> upstart_client_;
  FakeChromeUserManager* fake_user_manager_{new FakeChromeUserManager()};
  user_manager::ScopedUserManager scoped_user_manager_{
      base::WrapUnique(fake_user_manager_)};
  session_manager::SessionManager session_manager_;
  StrictMock<MockMojoDiagnosticsdService> mojo_diagnosticsd_service_;
};

// Test that wilco DTC support services are not started on enterprise enrolled
// devices with a certain device policy unset.
TEST_F(DiagnosticsdManagerTest, EnterpriseiWilcoDtcBasic) {
  DiagnosticsdManager diagnosticsd_manager(CreateDelegate());
  EXPECT_FALSE(DiagnosticsdBridge::Get());
}

// Test that wilco DTC support services are not started if disabled by device
// policy.
TEST_F(DiagnosticsdManagerTest, EnterpriseWilcoDtcDisabled) {
  DiagnosticsdManager diagnosticsd_manager(CreateDelegate());
  EXPECT_FALSE(DiagnosticsdBridge::Get());

  SetWilcoDtcAllowedPolicy(false);
  EXPECT_FALSE(DiagnosticsdBridge::Get());
}

// Test that wilco DTC support services are started if enabled by policy.
TEST_F(DiagnosticsdManagerTest, EnterpriseWilcoDtcAllowed) {
  SetWilcoDtcAllowedPolicy(true);
  DiagnosticsdManager diagnosticsd_manager(CreateDelegate());
  EXPECT_TRUE(DiagnosticsdBridge::Get());
}

// Test that wilco DTC support services are not started if non-affiliated user
// is logged-in.
TEST_F(DiagnosticsdManagerTest, EnterpriseNonAffiliatedUserLoggedIn) {
  DiagnosticsdManager diagnosticsd_manager(CreateDelegate());
  EXPECT_FALSE(DiagnosticsdBridge::Get());

  SetWilcoDtcAllowedPolicy(true);
  EXPECT_TRUE(DiagnosticsdBridge::Get());

  LogInUser(false);
  EXPECT_FALSE(DiagnosticsdBridge::Get());
}

// Test that wilco DTC support services are started if enabled by device policy
// and affiliated user is logged-in.
TEST_F(DiagnosticsdManagerTest, EnterpriseAffiliatedUserLoggedIn) {
  SetWilcoDtcAllowedPolicy(true);
  DiagnosticsdManager diagnosticsd_manager(CreateDelegate());
  EXPECT_TRUE(DiagnosticsdBridge::Get());

  LogInUser(true);
  EXPECT_TRUE(DiagnosticsdBridge::Get());
}

// Test that wilco DTC support services are not started if non-affiliated user
// is logged-in before the construction.
TEST_F(DiagnosticsdManagerTest, EnterpriseNonAffiliatedUserLoggedInBefore) {
  SetWilcoDtcAllowedPolicy(true);
  LogInUser(false);
  DiagnosticsdManager diagnosticsd_manager(CreateDelegate());

  EXPECT_FALSE(DiagnosticsdBridge::Get());
}

// Test that wilco DTC support services are properly notified about the changes
// of configuration data.
TEST_F(DiagnosticsdManagerTest, ConfigurationData) {
  constexpr char kFakeConfigurationData[] =
      "{\"fake-message\": \"Fake JSON configuration data\"}";

  DiagnosticsdManager diagnosticsd_manager(CreateDelegate());
  EXPECT_FALSE(DiagnosticsdBridge::Get());

  SetWilcoDtcAllowedPolicy(true);
  EXPECT_TRUE(DiagnosticsdBridge::Get());
  // An empty configuration data by default.
  EXPECT_TRUE(
      DiagnosticsdBridge::Get()->GetConfigurationDataForTesting().empty());

  // Set a non-empty configuration data.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*mojo_diagnosticsd_service(), NotifyConfigurationDataChanged())
        .WillOnce(Invoke([&run_loop]() { run_loop.Quit(); }));

    diagnosticsd_manager.SetConfigurationData(
        std::make_unique<std::string>(kFakeConfigurationData));
    EXPECT_EQ(kFakeConfigurationData,
              DiagnosticsdBridge::Get()->GetConfigurationDataForTesting());
    run_loop.Run();
  }

  // Restart the bridge.
  SetWilcoDtcAllowedPolicy(false);
  EXPECT_FALSE(DiagnosticsdBridge::Get());
  SetWilcoDtcAllowedPolicy(true);
  EXPECT_TRUE(DiagnosticsdBridge::Get());

  // The configuration data has not been changed.
  EXPECT_EQ(kFakeConfigurationData,
            DiagnosticsdBridge::Get()->GetConfigurationDataForTesting());

  // Clear the configuration data.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*mojo_diagnosticsd_service(), NotifyConfigurationDataChanged())
        .WillOnce(Invoke([&run_loop]() { run_loop.Quit(); }));
    diagnosticsd_manager.SetConfigurationData(nullptr);
    EXPECT_TRUE(
        DiagnosticsdBridge::Get()->GetConfigurationDataForTesting().empty());
    run_loop.Run();
  }
}

}  // namespace

}  // namespace chromeos

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/service.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "chromeos/services/assistant/fake_assistant_manager_service_impl.h"
#include "chromeos/services/assistant/public/mojom/constants.mojom.h"
#include "services/identity/public/mojom/identity_manager.mojom.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace assistant {

namespace {
constexpr base::TimeDelta kDefaultTokenExpirationDelay =
    base::TimeDelta::FromMilliseconds(1000);
}

class FakeIdentityManager : identity::mojom::IdentityManager {
 public:
  FakeIdentityManager()
      : binding_(this),
        access_token_expriation_delay_(kDefaultTokenExpirationDelay) {}

  identity::mojom::IdentityManagerPtr CreateInterfacePtrAndBind() {
    identity::mojom::IdentityManagerPtr ptr;
    binding_.Bind(mojo::MakeRequest(&ptr));
    return ptr;
  }

  void SetAccessTokenExpirationDelay(base::TimeDelta delay) {
    access_token_expriation_delay_ = delay;
  }

  void SetShouldFail(bool fail) { should_fail_ = fail; }

  int get_access_token_count() const { return get_access_token_count_; }

 private:
  // identity::mojom::IdentityManager:
  void GetPrimaryAccountInfo(GetPrimaryAccountInfoCallback callback) override {
    AccountInfo account_info;
    account_info.account_id = "account_id";
    account_info.gaia = "fakegaiaid";
    account_info.email = "fake@email";
    account_info.full_name = "full name";
    account_info.given_name = "given name";
    account_info.hosted_domain = "hosted_domain";
    account_info.locale = "en";
    account_info.picture_url = "http://fakepicture";

    identity::AccountState account_state;
    account_state.has_refresh_token = true;
    account_state.is_primary_account = true;

    std::move(callback).Run(account_info, account_state);
  }

  void GetPrimaryAccountWhenAvailable(
      GetPrimaryAccountWhenAvailableCallback callback) override {}
  void GetAccountInfoFromGaiaId(
      const std::string& gaia_id,
      GetAccountInfoFromGaiaIdCallback callback) override {}
  void GetAccounts(GetAccountsCallback callback) override {}
  void GetAccessToken(const std::string& account_id,
                      const ::identity::ScopeSet& scopes,
                      const std::string& consumer_id,
                      GetAccessTokenCallback callback) override {
    GoogleServiceAuthError auth_error(
        should_fail_ ? GoogleServiceAuthError::CONNECTION_FAILED
                     : GoogleServiceAuthError::NONE);
    std::move(callback).Run(
        should_fail_ ? base::nullopt
                     : base::Optional<std::string>("fake access token"),
        base::Time::Now() + access_token_expriation_delay_, auth_error);
    ++get_access_token_count_;
  }

  mojo::Binding<identity::mojom::IdentityManager> binding_;

  base::TimeDelta access_token_expriation_delay_;

  int get_access_token_count_ = 0;

  bool should_fail_ = false;

  DISALLOW_COPY_AND_ASSIGN(FakeIdentityManager);
};

class FakeAssistantClient : mojom::Client {
 public:
  FakeAssistantClient() : binding_(this) {}

  mojom::ClientPtr CreateInterfacePtrAndBind() {
    mojom::ClientPtr ptr;
    binding_.Bind(mojo::MakeRequest(&ptr));
    return ptr;
  }

 private:
  // mojom::Client:
  void OnAssistantStatusChanged(bool running) override {}
  void RequestAssistantStructure(
      RequestAssistantStructureCallback callback) override {}

  mojo::Binding<mojom::Client> binding_;

  DISALLOW_COPY_AND_ASSIGN(FakeAssistantClient);
};

class FakeDeviceActions : mojom::DeviceActions {
 public:
  FakeDeviceActions() : binding_(this) {}

  mojom::DeviceActionsPtr CreateInterfacePtrAndBind() {
    mojom::DeviceActionsPtr ptr;
    binding_.Bind(mojo::MakeRequest(&ptr));
    return ptr;
  }

 private:
  // mojom::DeviceActions:
  void SetWifiEnabled(bool enabled) override {}
  void SetBluetoothEnabled(bool enabled) override {}
  void GetScreenBrightnessLevel(
      GetScreenBrightnessLevelCallback callback) override {
    std::move(callback).Run(true, 1.0);
  }
  void SetScreenBrightnessLevel(double level, bool gradual) override {}
  void SetNightLightEnabled(bool enabled) override {}
  void OpenAndroidApp(chromeos::assistant::mojom::AndroidAppInfoPtr app_info,
                      OpenAndroidAppCallback callback) override {}
  void VerifyAndroidApp(
      std::vector<chromeos::assistant::mojom::AndroidAppInfoPtr> apps_info,
      VerifyAndroidAppCallback callback) override {}

  mojo::Binding<mojom::DeviceActions> binding_;

  DISALLOW_COPY_AND_ASSIGN(FakeDeviceActions);
};

class AssistantServiceTest : public testing::Test {
 public:
  AssistantServiceTest()
      : connector_(test_connector_factory_.CreateConnector()) {
    // The assistant service may attempt to connect to a number of services
    // which are irrelevant for these tests.
    test_connector_factory_.set_ignore_unknown_service_requests(true);

    auto power_manager_client =
        std::make_unique<chromeos::FakePowerManagerClient>();
    power_manager_client->SetTabletMode(
        chromeos::PowerManagerClient::TabletMode::OFF, base::TimeTicks());
    power_manager_client_ = power_manager_client.get();

    auto dbus_setter = chromeos::DBusThreadManager::GetSetterForTesting();
    dbus_setter->SetPowerManagerClient(std::move(power_manager_client));

    service_ = std::make_unique<Service>(
        test_connector_factory_.RegisterInstance(mojom::kServiceName),
        nullptr /* network_connection_tracker */, nullptr /* io_task_runner */);

    mock_task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
        base::Time::Now(), base::TimeTicks::Now());
    auto mock_timer = std::make_unique<base::OneShotTimer>(
        mock_task_runner_->GetMockTickClock());
    mock_timer->SetTaskRunner(mock_task_runner_);
    service_->SetTimerForTesting(std::move(mock_timer));

    service_->SetIdentityManagerForTesting(
        fake_identity_manager_.CreateInterfacePtrAndBind());

    auto fake_assistant_manager =
        std::make_unique<FakeAssistantManagerServiceImpl>();
    fake_assistant_manager_ = fake_assistant_manager.get();
    service_->SetAssistantManagerForTesting(std::move(fake_assistant_manager));
  }

  void SetUp() override {
    GetPlatform()->Init(fake_assistant_client_.CreateInterfacePtrAndBind(),
                        fake_device_actions_.CreateInterfacePtrAndBind());
    platform_.FlushForTesting();
    base::RunLoop().RunUntilIdle();
  }

  mojom::AssistantPlatform* GetPlatform() {
    if (!platform_)
      connector_->BindInterface(mojom::kServiceName, &platform_);
    return platform_.get();
  }

  FakeIdentityManager* identity_manager() { return &fake_identity_manager_; }

  FakeAssistantManagerServiceImpl* assistant_manager_service() {
    return fake_assistant_manager_;
  }

  chromeos::FakePowerManagerClient* power_manager_client() {
    return power_manager_client_;
  }

  base::TestMockTimeTaskRunner* mock_task_runner() {
    return mock_task_runner_.get();
  }

 private:
  base::test::ScopedTaskEnvironment task_environment_;
  service_manager::TestConnectorFactory test_connector_factory_;
  std::unique_ptr<service_manager::Connector> connector_;

  std::unique_ptr<chromeos::assistant::Service> service_;
  mojom::AssistantPlatformPtr platform_;

  FakeIdentityManager fake_identity_manager_;
  FakeAssistantClient fake_assistant_client_;
  FakeDeviceActions fake_device_actions_;

  FakeAssistantManagerServiceImpl* fake_assistant_manager_;
  chromeos::FakePowerManagerClient* power_manager_client_;

  scoped_refptr<base::TestMockTimeTaskRunner> mock_task_runner_;
  std::unique_ptr<base::OneShotTimer> mock_timer_;

  DISALLOW_COPY_AND_ASSIGN(AssistantServiceTest);
};

TEST_F(AssistantServiceTest, RefreshTokenAfterExpire) {
  auto current_count = identity_manager()->get_access_token_count();
  mock_task_runner()->FastForwardBy(kDefaultTokenExpirationDelay / 2);
  base::RunLoop().RunUntilIdle();

  // Before token expire, should not request new token.
  EXPECT_EQ(identity_manager()->get_access_token_count(), current_count);

  mock_task_runner()->FastForwardBy(kDefaultTokenExpirationDelay);
  base::RunLoop().RunUntilIdle();

  // After token expire, should request once.
  EXPECT_EQ(identity_manager()->get_access_token_count(), ++current_count);
}

TEST_F(AssistantServiceTest, RetryRefreshTokenAfterFailure) {
  auto current_count = identity_manager()->get_access_token_count();
  identity_manager()->SetShouldFail(true);
  mock_task_runner()->FastForwardBy(kDefaultTokenExpirationDelay);
  base::RunLoop().RunUntilIdle();

  // Token request failed.
  EXPECT_EQ(identity_manager()->get_access_token_count(), ++current_count);

  base::RunLoop().RunUntilIdle();

  // Token request automatically retry.
  identity_manager()->SetShouldFail(false);
  // The failure delay has jitter so fast forward a bit more.
  mock_task_runner()->FastForwardBy(kDefaultTokenExpirationDelay * 2);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(identity_manager()->get_access_token_count(), ++current_count);
}

TEST_F(AssistantServiceTest, RetryRefreshTokenAfterDeviceWakeup) {
  auto current_count = identity_manager()->get_access_token_count();
  power_manager_client()->SendSuspendDone();
  base::RunLoop().RunUntilIdle();

  // Token requested immediately after suspend done.
  EXPECT_EQ(identity_manager()->get_access_token_count(), ++current_count);
}

}  // namespace assistant
}  // namespace chromeos

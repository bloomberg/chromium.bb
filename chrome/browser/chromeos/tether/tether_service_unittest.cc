// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/tether/tether_service.h"

#include <memory>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/chromeos/net/tether_notification_presenter.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/components/tether/fake_notification_presenter.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "chromeos/dbus/fake_shill_manager_client.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/network/network_connect.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_test.h"
#include "chromeos/network/network_type_pattern.h"
#include "components/cryptauth/cryptauth_device_manager.h"
#include "components/cryptauth/fake_cryptauth_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center.h"

using testing::Invoke;
using testing::NiceMock;
using testing::Return;

namespace {

class ExtendedFakeSessionManagerClient
    : public chromeos::FakeSessionManagerClient {
 public:
  bool IsScreenLocked() const override { return is_screen_locked_; }

  void set_is_screen_locked(bool is_screen_locked) {
    is_screen_locked_ = is_screen_locked;
  }

 private:
  bool is_screen_locked_ = false;
};

class MockCryptAuthDeviceManager : public cryptauth::CryptAuthDeviceManager {
 public:
  ~MockCryptAuthDeviceManager() override {}

  MOCK_CONST_METHOD0(GetTetherHosts,
                     std::vector<cryptauth::ExternalDeviceInfo>());
};

class MockExtendedBluetoothAdapter : public device::MockBluetoothAdapter {
 public:
  void SetAdvertisingInterval(
      const base::TimeDelta& min,
      const base::TimeDelta& max,
      const base::Closure& callback,
      const AdvertisementErrorCallback& error_callback) override {
    if (is_ble_advertising_supported_) {
      callback.Run();
    } else {
      error_callback.Run(device::BluetoothAdvertisement::ErrorCode::
                             ERROR_INVALID_ADVERTISEMENT_INTERVAL);
    }
  }

  void set_is_ble_advertising_supported(bool is_ble_advertising_supported) {
    is_ble_advertising_supported_ = is_ble_advertising_supported;
  }

 protected:
  ~MockExtendedBluetoothAdapter() override {}

 private:
  bool is_ble_advertising_supported_ = true;
};

class TestTetherService : public TetherService {
 public:
  TestTetherService(Profile* profile,
                    chromeos::PowerManagerClient* power_manager_client,
                    chromeos::SessionManagerClient* session_manager_client,
                    cryptauth::CryptAuthService* cryptauth_service,
                    chromeos::NetworkStateHandler* network_state_handler)
      : TetherService(profile,
                      power_manager_client,
                      session_manager_client,
                      cryptauth_service,
                      network_state_handler) {}
  ~TestTetherService() override {}

  int updated_technology_state_count() {
    return updated_technology_state_count_;
  }

 protected:
  void UpdateTetherTechnologyState() override {
    updated_technology_state_count_++;
    TetherService::UpdateTetherTechnologyState();
  }

 private:
  int updated_technology_state_count_ = 0;
};

class TestInitializerDelegate : public TetherService::InitializerDelegate {
 public:
  bool is_tether_running() { return is_tether_running_; }

  // TetherService::InitializerDelegate:
  void InitializeTether(
      cryptauth::CryptAuthService* cryptauth_service,
      chromeos::tether::NotificationPresenter* notification_presenter,
      PrefService* pref_service,
      ProfileOAuth2TokenService* token_service,
      chromeos::NetworkStateHandler* network_state_handler,
      chromeos::ManagedNetworkConfigurationHandler*
          managed_network_configuration_handler,
      chromeos::NetworkConnect* network_connect,
      chromeos::NetworkConnectionHandler* network_connection_handler,
      scoped_refptr<device::BluetoothAdapter> adapter) override {
    is_tether_running_ = true;
  }

  void ShutdownTether() override { is_tether_running_ = false; }

 private:
  bool is_tether_running_ = false;
};

}  // namespace

class TetherServiceTest : public chromeos::NetworkStateTest {
 protected:
  TetherServiceTest() : NetworkStateTest() {}
  ~TetherServiceTest() override {}

  void SetUp() override {
    chromeos::DBusThreadManager::Initialize();
    chromeos::NetworkStateTest::SetUp();

    message_center::MessageCenter::Initialize();
    chromeos::NetworkConnect::Initialize(nullptr);
    chromeos::NetworkHandler::Initialize();

    TestingProfile::Builder builder;
    profile_ = builder.Build();

    fake_power_manager_client_ =
        base::MakeUnique<chromeos::FakePowerManagerClient>();
    fake_session_manager_client_ =
        base::MakeUnique<ExtendedFakeSessionManagerClient>();

    std::vector<cryptauth::ExternalDeviceInfo> test_device_infos;
    test_device_infos.push_back(cryptauth::ExternalDeviceInfo());
    test_device_infos.push_back(cryptauth::ExternalDeviceInfo());
    mock_cryptauth_device_manager_ =
        base::WrapUnique(new NiceMock<MockCryptAuthDeviceManager>());
    ON_CALL(*mock_cryptauth_device_manager_, GetTetherHosts())
        .WillByDefault(Return(test_device_infos));
    fake_cryptauth_service_ =
        base::MakeUnique<cryptauth::FakeCryptAuthService>();
    fake_cryptauth_service_->set_cryptauth_device_manager(
        mock_cryptauth_device_manager_.get());

    mock_adapter_ =
        make_scoped_refptr(new NiceMock<MockExtendedBluetoothAdapter>());
    SetIsBluetoothPowered(true);
    ON_CALL(*mock_adapter_, IsPresent()).WillByDefault(Return(true));
    ON_CALL(*mock_adapter_, IsPowered())
        .WillByDefault(Invoke(this, &TetherServiceTest::IsBluetoothPowered));
    device::BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter_);
  }

  void TearDown() override {
    ShutdownTetherService();

    message_center::MessageCenter::Shutdown();
    chromeos::NetworkConnect::Shutdown();
    chromeos::NetworkHandler::Shutdown();

    ShutdownNetworkState();
    chromeos::NetworkStateTest::TearDown();
    chromeos::DBusThreadManager::Shutdown();
  }

  void CreateTetherService() {
    tether_service_ = base::WrapUnique(new TestTetherService(
        profile_.get(), fake_power_manager_client_.get(),
        fake_session_manager_client_.get(), fake_cryptauth_service_.get(),
        network_state_handler()));

    test_initializer_delegate_ = new TestInitializerDelegate();
    tether_service_->SetInitializerDelegateForTest(
        base::WrapUnique(test_initializer_delegate_));

    fake_notification_presenter_ =
        new chromeos::tether::FakeNotificationPresenter();
    tether_service_->SetNotificationPresenterForTest(
        base::WrapUnique(fake_notification_presenter_));

    // Ensure that TetherService does not prematurely update its TechnologyState
    // before it fetches the BluetoothAdapter.
    EXPECT_EQ(
        chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
        network_state_handler()->GetTechnologyState(
            chromeos::NetworkTypePattern::Tether()));
    EXPECT_FALSE(test_initializer_delegate_->is_tether_running());

    base::RunLoop().RunUntilIdle();
  }

  void ShutdownTetherService() {
    if (tether_service_)
      tether_service_->Shutdown();
  }

  void SetIsScreenLocked(bool is_screen_locked) {
    fake_session_manager_client_->set_is_screen_locked(is_screen_locked);
    if (is_screen_locked)
      tether_service_->ScreenIsLocked();
    else
      tether_service_->ScreenIsUnlocked();
  }

  void SetTetherTechnologyStateEnabled(bool enabled) {
    network_state_handler()->SetTetherTechnologyState(
        enabled
            ? chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED
            : chromeos::NetworkStateHandler::TechnologyState::
                  TECHNOLOGY_AVAILABLE);
  }

  void SetCellularTechnologyStateEnabled(bool enabled) {
    network_state_handler()->SetTechnologyEnabled(
        chromeos::NetworkTypePattern::Cellular(), enabled,
        chromeos::network_handler::ErrorCallback());
    base::RunLoop().RunUntilIdle();
  }

  void SetIsBluetoothPowered(bool powered) {
    is_adapter_powered_ = powered;
    for (auto& observer : mock_adapter_->GetObservers())
      observer.AdapterPoweredChanged(mock_adapter_.get(), powered);
  }

  bool IsBluetoothPowered() { return is_adapter_powered_; }

  void DisconnectDefaultShillNetworks() {
    const chromeos::NetworkState* default_state;
    while ((default_state = network_state_handler()->DefaultNetwork())) {
      SetServiceProperty(default_state->path(), shill::kStateProperty,
                         base::Value(shill::kStateIdle));
    }
  }

  void ShutdownAndVerifyFinalTetherFeatureState(
      TetherService::TetherFeatureState expected_technology_state_and_reason) {
    ShutdownTetherService();

    histogram_tester_.ExpectUniqueSample("InstantTethering.FinalFeatureState",
                                         expected_technology_state_and_reason,
                                         1);
  }

  const content::TestBrowserThreadBundle thread_bundle_;

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<chromeos::FakePowerManagerClient> fake_power_manager_client_;
  std::unique_ptr<ExtendedFakeSessionManagerClient>
      fake_session_manager_client_;
  std::unique_ptr<TestingPrefServiceSimple> test_pref_service_;
  std::unique_ptr<NiceMock<MockCryptAuthDeviceManager>>
      mock_cryptauth_device_manager_;
  TestInitializerDelegate* test_initializer_delegate_;
  chromeos::tether::FakeNotificationPresenter* fake_notification_presenter_;
  std::unique_ptr<cryptauth::FakeCryptAuthService> fake_cryptauth_service_;

  scoped_refptr<MockExtendedBluetoothAdapter> mock_adapter_;
  bool is_adapter_powered_;

  std::unique_ptr<TestTetherService> tether_service_;

  base::HistogramTester histogram_tester_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TetherServiceTest);
};

TEST_F(TetherServiceTest, TestShutdown) {
  CreateTetherService();
  EXPECT_TRUE(test_initializer_delegate_->is_tether_running());

  ShutdownTetherService();

  // The TechnologyState should not have changed due to Shutdown() being called.
  // If it had changed, any settings UI that was previously open would have
  // shown visual jank.
  EXPECT_EQ(chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                chromeos::NetworkTypePattern::Tether()));
  EXPECT_FALSE(test_initializer_delegate_->is_tether_running());
}

TEST_F(TetherServiceTest, TestSuspend) {
  CreateTetherService();
  EXPECT_TRUE(test_initializer_delegate_->is_tether_running());

  fake_power_manager_client_->SendSuspendImminent();

  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));
  EXPECT_FALSE(test_initializer_delegate_->is_tether_running());

  fake_power_manager_client_->SendSuspendDone();

  EXPECT_EQ(chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                chromeos::NetworkTypePattern::Tether()));
  EXPECT_TRUE(test_initializer_delegate_->is_tether_running());

  fake_power_manager_client_->SendSuspendImminent();

  ShutdownAndVerifyFinalTetherFeatureState(
      TetherService::TetherFeatureState::OTHER_OR_UNKNOWN);
}

TEST_F(TetherServiceTest, TestBleAdvertisingNotSupported) {
  mock_adapter_->set_is_ble_advertising_supported(false);

  CreateTetherService();

  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));
  EXPECT_FALSE(test_initializer_delegate_->is_tether_running());

  ShutdownAndVerifyFinalTetherFeatureState(
      TetherService::TetherFeatureState::BLE_ADVERTISING_NOT_SUPPORTED);
}

TEST_F(TetherServiceTest,
       TestBleAdvertisingNotSupported_BluetoothIsInitiallyNotPowered) {
  SetIsBluetoothPowered(false);

  mock_adapter_->set_is_ble_advertising_supported(false);

  CreateTetherService();

  // TetherService has not yet been able to find out that BLE advertising is not
  // supported.
  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_UNINITIALIZED,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));
  EXPECT_FALSE(test_initializer_delegate_->is_tether_running());
  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(
      prefs::kInstantTetheringBleAdvertisingSupported));

  SetIsBluetoothPowered(true);

  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));
  EXPECT_FALSE(test_initializer_delegate_->is_tether_running());
  EXPECT_FALSE(profile_->GetPrefs()->GetBoolean(
      prefs::kInstantTetheringBleAdvertisingSupported));

  ShutdownAndVerifyFinalTetherFeatureState(
      TetherService::TetherFeatureState::BLE_ADVERTISING_NOT_SUPPORTED);
}

TEST_F(
    TetherServiceTest,
    TestBleAdvertisingNotSupportedAndRecorded_BluetoothIsInitiallyNotPowered) {
  SetIsBluetoothPowered(false);

  mock_adapter_->set_is_ble_advertising_supported(false);

  // Simulate a login after we determined that BLE advertising is not supported.
  profile_->GetPrefs()->SetBoolean(
      prefs::kInstantTetheringBleAdvertisingSupported, false);

  CreateTetherService();

  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));
  EXPECT_FALSE(test_initializer_delegate_->is_tether_running());

  SetIsBluetoothPowered(true);

  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));
  EXPECT_FALSE(test_initializer_delegate_->is_tether_running());

  ShutdownAndVerifyFinalTetherFeatureState(
      TetherService::TetherFeatureState::BLE_ADVERTISING_NOT_SUPPORTED);
}

TEST_F(TetherServiceTest, TestBleAdvertisingSupportedButIncorrectlyRecorded) {
  // Simulate a login after we incorrectly determined that BLE advertising is
  // not supported (this is not an expected case, but may have happened if
  // BluetoothAdapter::SetAdvertisingInterval() failed for a weird, one-off
  // reason).
  profile_->GetPrefs()->SetBoolean(
      prefs::kInstantTetheringBleAdvertisingSupported, false);

  CreateTetherService();

  EXPECT_EQ(chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                chromeos::NetworkTypePattern::Tether()));
  EXPECT_TRUE(test_initializer_delegate_->is_tether_running());
  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(
      prefs::kInstantTetheringBleAdvertisingSupported));

  ShutdownAndVerifyFinalTetherFeatureState(
      TetherService::TetherFeatureState::ENABLED);
}

TEST_F(TetherServiceTest, TestScreenLock) {
  CreateTetherService();
  EXPECT_TRUE(test_initializer_delegate_->is_tether_running());

  SetIsScreenLocked(true);

  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));
  EXPECT_FALSE(test_initializer_delegate_->is_tether_running());

  SetIsScreenLocked(false);

  EXPECT_EQ(chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                chromeos::NetworkTypePattern::Tether()));
  EXPECT_TRUE(test_initializer_delegate_->is_tether_running());

  SetIsScreenLocked(true);

  ShutdownAndVerifyFinalTetherFeatureState(
      TetherService::TetherFeatureState::SCREEN_LOCKED);
}

TEST_F(TetherServiceTest, TestFeatureFlagDisabled) {
  EXPECT_FALSE(TetherService::Get(profile_.get()));
}

TEST_F(TetherServiceTest, TestFeatureFlagEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kInstantTethering);

  TetherService* tether_service = TetherService::Get(profile_.get());
  ASSERT_TRUE(tether_service);

  base::RunLoop().RunUntilIdle();
  tether_service->Shutdown();
}

TEST_F(TetherServiceTest, TestNoTetherHosts) {
  ON_CALL(*mock_cryptauth_device_manager_, GetTetherHosts())
      .WillByDefault(Return(std::vector<cryptauth::ExternalDeviceInfo>()));

  CreateTetherService();

  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));
  EXPECT_FALSE(test_initializer_delegate_->is_tether_running());

  ShutdownAndVerifyFinalTetherFeatureState(
      TetherService::TetherFeatureState::NO_AVAILABLE_HOSTS);
}

TEST_F(TetherServiceTest, TestProhibitedByPolicy) {
  profile_->GetPrefs()->SetBoolean(prefs::kInstantTetheringAllowed, false);

  CreateTetherService();

  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_PROHIBITED,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));
  EXPECT_FALSE(test_initializer_delegate_->is_tether_running());

  ShutdownAndVerifyFinalTetherFeatureState(
      TetherService::TetherFeatureState::PROHIBITED);
}

TEST_F(TetherServiceTest, TestIsBluetoothPowered) {
  SetIsBluetoothPowered(false);

  CreateTetherService();

  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_UNINITIALIZED,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));
  EXPECT_FALSE(test_initializer_delegate_->is_tether_running());

  SetIsBluetoothPowered(true);

  EXPECT_EQ(chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                chromeos::NetworkTypePattern::Tether()));
  EXPECT_TRUE(test_initializer_delegate_->is_tether_running());

  SetIsBluetoothPowered(false);

  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_UNINITIALIZED,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));
  EXPECT_FALSE(test_initializer_delegate_->is_tether_running());

  ShutdownAndVerifyFinalTetherFeatureState(
      TetherService::TetherFeatureState::BLUETOOTH_DISABLED);
}

TEST_F(TetherServiceTest, TestCellularIsUnavailable) {
  test_manager_client()->RemoveTechnology(shill::kTypeCellular);
  ASSERT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Cellular()));

  CreateTetherService();

  SetTetherTechnologyStateEnabled(false);
  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_AVAILABLE,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));
  EXPECT_FALSE(test_initializer_delegate_->is_tether_running());

  SetTetherTechnologyStateEnabled(true);
  EXPECT_EQ(chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                chromeos::NetworkTypePattern::Tether()));
  EXPECT_TRUE(test_initializer_delegate_->is_tether_running());

  ShutdownAndVerifyFinalTetherFeatureState(
      TetherService::TetherFeatureState::ENABLED);
}

TEST_F(TetherServiceTest, TestCellularIsAvailable) {
  // TODO (lesliewatkins): Investigate why cellular needs to be removed and
  // re-added for NetworkStateHandler to return the correct TechnologyState.
  test_manager_client()->RemoveTechnology(shill::kTypeCellular);
  test_manager_client()->AddTechnology(shill::kTypeCellular, false);

  CreateTetherService();

  // Cellular disabled
  SetCellularTechnologyStateEnabled(false);
  ASSERT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_AVAILABLE,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Cellular()));
  EXPECT_FALSE(test_initializer_delegate_->is_tether_running());

  SetTetherTechnologyStateEnabled(false);
  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));
  EXPECT_FALSE(test_initializer_delegate_->is_tether_running());

  SetTetherTechnologyStateEnabled(true);
  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));
  EXPECT_FALSE(test_initializer_delegate_->is_tether_running());

  // Cellular enabled
  SetCellularTechnologyStateEnabled(true);
  ASSERT_EQ(chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                chromeos::NetworkTypePattern::Cellular()));
  EXPECT_TRUE(test_initializer_delegate_->is_tether_running());

  SetTetherTechnologyStateEnabled(false);
  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_AVAILABLE,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));
  EXPECT_FALSE(test_initializer_delegate_->is_tether_running());

  SetTetherTechnologyStateEnabled(true);
  EXPECT_EQ(chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                chromeos::NetworkTypePattern::Tether()));
  EXPECT_TRUE(test_initializer_delegate_->is_tether_running());

  SetCellularTechnologyStateEnabled(false);

  ShutdownAndVerifyFinalTetherFeatureState(
      TetherService::TetherFeatureState::CELLULAR_DISABLED);
}

TEST_F(TetherServiceTest, TestDisabled) {
  profile_->GetPrefs()->SetBoolean(prefs::kInstantTetheringEnabled, false);

  CreateTetherService();

  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_AVAILABLE,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));
  EXPECT_FALSE(
      profile_->GetPrefs()->GetBoolean(prefs::kInstantTetheringEnabled));
  EXPECT_FALSE(test_initializer_delegate_->is_tether_running());

  ShutdownAndVerifyFinalTetherFeatureState(
      TetherService::TetherFeatureState::USER_PREFERENCE_DISABLED);
}

TEST_F(TetherServiceTest, TestEnabled) {
  CreateTetherService();

  EXPECT_EQ(chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                chromeos::NetworkTypePattern::Tether()));
  EXPECT_TRUE(test_initializer_delegate_->is_tether_running());

  SetTetherTechnologyStateEnabled(false);
  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_AVAILABLE,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));
  EXPECT_FALSE(
      profile_->GetPrefs()->GetBoolean(prefs::kInstantTetheringEnabled));
  EXPECT_FALSE(test_initializer_delegate_->is_tether_running());

  SetTetherTechnologyStateEnabled(true);
  EXPECT_EQ(chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                chromeos::NetworkTypePattern::Tether()));
  EXPECT_TRUE(
      profile_->GetPrefs()->GetBoolean(prefs::kInstantTetheringEnabled));
  EXPECT_TRUE(test_initializer_delegate_->is_tether_running());

  ShutdownAndVerifyFinalTetherFeatureState(
      TetherService::TetherFeatureState::ENABLED);
}

// Test against a past defect that made TetherService and NetworkStateHandler
// repeatly update technology state after the other did so. TetherService should
// only update technology state if NetworkStateHandler has provided a different
// state than the user preference.
TEST_F(TetherServiceTest, TestEnabledMultipleChanges) {
  CreateTetherService();
  // CreateTetherService calls RunUntilIdle() so UpdateTetherTechnologyState()
  // may be called multiple times in the initialization process.
  int updated_technology_state_count =
      tether_service_->updated_technology_state_count();

  SetTetherTechnologyStateEnabled(false);
  SetTetherTechnologyStateEnabled(false);
  SetTetherTechnologyStateEnabled(false);

  updated_technology_state_count++;
  EXPECT_EQ(updated_technology_state_count,
            tether_service_->updated_technology_state_count());

  SetTetherTechnologyStateEnabled(true);
  SetTetherTechnologyStateEnabled(true);
  SetTetherTechnologyStateEnabled(true);

  updated_technology_state_count++;
  EXPECT_EQ(updated_technology_state_count,
            tether_service_->updated_technology_state_count());
}

TEST_F(TetherServiceTest, TestBluetoothNotification) {
  SetIsBluetoothPowered(false);

  CreateTetherService();
  DisconnectDefaultShillNetworks();

  // The notification should be visible since there is no active network and
  // Bluetooth is disabled when the service started up.
  EXPECT_TRUE(
      fake_notification_presenter_->is_enable_bluetooth_notification_shown());

  // Now, simulate the adapter being turned on. The notification should no
  // longer be visible.
  SetIsBluetoothPowered(true);
  EXPECT_FALSE(
      fake_notification_presenter_->is_enable_bluetooth_notification_shown());

  // Now, simulate the adapter being turned off again. The notification should
  // still *not* be available. It should only be shown when the service starts
  // up or when the network has been disconnected.
  SetIsBluetoothPowered(false);
  EXPECT_FALSE(
      fake_notification_presenter_->is_enable_bluetooth_notification_shown());

  // Now, connect to the default Ethernet network. The notification still should
  // not be shown.
  SetServiceProperty(
      network_state_handler()
          ->GetNetworkStateFromGuid(
              chromeos::FakeShillManagerClient::kFakeEthernetNetworkGuid)
          ->path(),
      shill::kStateProperty, base::Value(shill::kStateOnline));
  EXPECT_FALSE(
      fake_notification_presenter_->is_enable_bluetooth_notification_shown());

  // Now, disconnect. The notification should be shown again.
  SetServiceProperty(
      network_state_handler()
          ->GetNetworkStateFromGuid(
              chromeos::FakeShillManagerClient::kFakeEthernetNetworkGuid)
          ->path(),
      shill::kStateProperty, base::Value(shill::kStateIdle));
  EXPECT_TRUE(
      fake_notification_presenter_->is_enable_bluetooth_notification_shown());

  // Now, disable the Tether preference. The notification should be hidden
  // again.
  SetTetherTechnologyStateEnabled(false);
  EXPECT_FALSE(
      fake_notification_presenter_->is_enable_bluetooth_notification_shown());
}

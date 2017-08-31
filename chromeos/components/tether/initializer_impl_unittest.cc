// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/initializer_impl.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/components/tether/active_host.h"
#include "chromeos/components/tether/fake_notification_presenter.h"
#include "chromeos/components/tether/host_scan_device_prioritizer.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/mock_managed_network_configuration_handler.h"
#include "chromeos/network/network_connect.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state_test.h"
#include "components/cryptauth/cryptauth_device_manager.h"
#include "components/cryptauth/cryptauth_enroller.h"
#include "components/cryptauth/cryptauth_enrollment_manager.h"
#include "components/cryptauth/fake_cryptauth_gcm_manager.h"
#include "components/cryptauth/fake_cryptauth_service.h"
#include "components/cryptauth/proto/cryptauth_api.pb.h"
#include "components/cryptauth/secure_message_delegate.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

using testing::NiceMock;

namespace chromeos {

namespace tether {

namespace {

class MockCryptAuthDeviceManager : public cryptauth::CryptAuthDeviceManager {
 public:
  ~MockCryptAuthDeviceManager() override {}

  MOCK_CONST_METHOD0(GetTetherHosts,
                     std::vector<cryptauth::ExternalDeviceInfo>());
};

class MockCryptAuthEnrollmentManager
    : public cryptauth::CryptAuthEnrollmentManager {
 public:
  explicit MockCryptAuthEnrollmentManager(
      cryptauth::FakeCryptAuthGCMManager* fake_cryptauth_gcm_manager)
      : cryptauth::CryptAuthEnrollmentManager(
            nullptr /* clock */,
            nullptr /* enroller_factory */,
            nullptr /* secure_message_delegate */,
            cryptauth::GcmDeviceInfo(),
            fake_cryptauth_gcm_manager,
            nullptr /* pref_service */) {}
  ~MockCryptAuthEnrollmentManager() override {}

  MOCK_CONST_METHOD0(GetUserPrivateKey, std::string());
};

class MockNetworkConnect : public NetworkConnect {
 public:
  MockNetworkConnect() : NetworkConnect() {}
  ~MockNetworkConnect() override {}

  MOCK_METHOD1(ConnectToNetworkId, void(const std::string&));
  MOCK_METHOD1(DisconnectFromNetworkId, void(const std::string&));
  MOCK_METHOD2(MaybeShowConfigureUI,
               bool(const std::string&, const std::string&));
  MOCK_METHOD2(SetTechnologyEnabled,
               void(const chromeos::NetworkTypePattern&, bool));
  MOCK_METHOD1(ShowMobileSetup, void(const std::string&));
  MOCK_METHOD3(ConfigureNetworkIdAndConnect,
               void(const std::string&, const base::DictionaryValue&, bool));
  MOCK_METHOD2(CreateConfigurationAndConnect,
               void(base::DictionaryValue*, bool));
  MOCK_METHOD2(CreateConfiguration, void(base::DictionaryValue*, bool));
};

class TestNetworkConnectionHandler : public NetworkConnectionHandler {
 public:
  TestNetworkConnectionHandler() : NetworkConnectionHandler() {}
  ~TestNetworkConnectionHandler() override {}

  // NetworkConnectionHandler:
  void ConnectToNetwork(const std::string& service_path,
                        const base::Closure& success_callback,
                        const network_handler::ErrorCallback& error_callback,
                        bool check_error_state) override {}

  void DisconnectNetwork(
      const std::string& service_path,
      const base::Closure& success_callback,
      const network_handler::ErrorCallback& error_callback) override {}

  bool HasConnectingNetwork(const std::string& service_path) override {
    return false;
  }

  bool HasPendingConnectRequest() override { return false; }

  void Init(NetworkStateHandler* network_state_handler,
            NetworkConfigurationHandler* network_configuration_handler,
            ManagedNetworkConfigurationHandler*
                managed_network_configuration_handler) override {}
};

}  // namespace

class InitializerTest : public NetworkStateTest {
 protected:
  InitializerTest() : NetworkStateTest() {}
  ~InitializerTest() override {}

  void SetUp() override {
    DBusThreadManager::Initialize();
    NetworkHandler::Initialize();
    NetworkStateTest::SetUp();

    network_state_handler()->SetTetherTechnologyState(
        NetworkStateHandler::TECHNOLOGY_ENABLED);

    test_pref_service_ = base::MakeUnique<TestingPrefServiceSimple>();
    InitializerImpl::RegisterProfilePrefs(test_pref_service_->registry());
  }

  void TearDown() override {
    ShutdownNetworkState();
    NetworkStateTest::TearDown();
    DBusThreadManager::Shutdown();
  }

  void InitializeAndDestroy(
      cryptauth::CryptAuthService* cryptauth_service,
      NotificationPresenter* notification_presenter,
      PrefService* pref_service,
      ProfileOAuth2TokenService* token_service,
      NetworkStateHandler* network_state_handler,
      ManagedNetworkConfigurationHandler* managed_network_configuration_handler,
      NetworkConnect* network_connect,
      NetworkConnectionHandler* network_connection_handler,
      scoped_refptr<device::BluetoothAdapter> adapter) {
    Initializer* initializer = new InitializerImpl(
        cryptauth_service, notification_presenter, pref_service, token_service,
        network_state_handler, managed_network_configuration_handler,
        network_connect, network_connection_handler, adapter);
    delete initializer;
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;

  std::unique_ptr<TestingPrefServiceSimple> test_pref_service_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InitializerTest);
};

// This test ensures that Initializer's destructor runs in the correct order and
// results in a correct clean-up of all created components. If the destructor
// were to result in an error being thrown, this test would fail.
TEST_F(InitializerTest, TestCreateAndDestroy) {
  std::unique_ptr<NiceMock<MockCryptAuthDeviceManager>> mock_device_manager =
      base::WrapUnique(new NiceMock<MockCryptAuthDeviceManager>());

  std::unique_ptr<cryptauth::FakeCryptAuthGCMManager>
      fake_cryptauth_gcm_manager =
          base::MakeUnique<cryptauth::FakeCryptAuthGCMManager>(
              "registrationId");

  std::unique_ptr<NiceMock<MockCryptAuthEnrollmentManager>>
      mock_enrollment_manager =
          base::WrapUnique(new NiceMock<MockCryptAuthEnrollmentManager>(
              fake_cryptauth_gcm_manager.get()));

  std::unique_ptr<cryptauth::FakeCryptAuthService> fake_cryptauth_service =
      base::MakeUnique<cryptauth::FakeCryptAuthService>();
  fake_cryptauth_service->set_cryptauth_device_manager(
      mock_device_manager.get());
  fake_cryptauth_service->set_cryptauth_enrollment_manager(
      mock_enrollment_manager.get());

  std::unique_ptr<FakeNotificationPresenter> fake_notification_presenter =
      base::MakeUnique<FakeNotificationPresenter>();

  std::unique_ptr<TestingPrefServiceSimple> test_pref_service =
      base::MakeUnique<TestingPrefServiceSimple>();

  std::unique_ptr<FakeProfileOAuth2TokenService> fake_token_service =
      base::MakeUnique<FakeProfileOAuth2TokenService>();

  std::unique_ptr<ManagedNetworkConfigurationHandler>
      managed_network_configuration_handler = base::WrapUnique(
          new NiceMock<MockManagedNetworkConfigurationHandler>);

  std::unique_ptr<MockNetworkConnect> mock_network_connect =
      base::WrapUnique(new NiceMock<MockNetworkConnect>);

  std::unique_ptr<NetworkConnectionHandler> network_connection_handler_ =
      base::MakeUnique<TestNetworkConnectionHandler>();

  scoped_refptr<NiceMock<device::MockBluetoothAdapter>> mock_adapter =
      make_scoped_refptr(new NiceMock<device::MockBluetoothAdapter>());

  // Call an instance method of the test instead of initializing and destroying
  // here because the friend relationship between Initializer and
  // InitializerTest only applies to the class itself, not these test functions.
  InitializeAndDestroy(
      fake_cryptauth_service.get(), fake_notification_presenter.get(),
      test_pref_service_.get(), fake_token_service.get(),
      network_state_handler(), managed_network_configuration_handler.get(),
      mock_network_connect.get(), network_connection_handler_.get(),
      mock_adapter);
}

}  // namespace tether

}  // namespace chromeos

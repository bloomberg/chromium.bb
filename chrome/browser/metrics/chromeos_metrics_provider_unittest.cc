// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chromeos_metrics_provider.h"

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/chromeos/login/users/fake_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/metrics/chromeos_metrics_provider.h"
#include "chromeos/dbus/fake_bluetooth_adapter_client.h"
#include "chromeos/dbus/fake_bluetooth_agent_manager_client.h"
#include "chromeos/dbus/fake_bluetooth_device_client.h"
#include "chromeos/dbus/fake_bluetooth_gatt_characteristic_client.h"
#include "chromeos/dbus/fake_bluetooth_gatt_descriptor_client.h"
#include "chromeos/dbus/fake_bluetooth_gatt_service_client.h"
#include "chromeos/dbus/fake_bluetooth_input_client.h"
#include "chromeos/dbus/fake_dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/login/login_state.h"
#include "components/metrics/proto/system_profile.pb.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_X11)
#include "ui/events/x/device_data_manager_x11.h"
#endif

using chromeos::DBusThreadManager;
using chromeos::BluetoothAdapterClient;
using chromeos::BluetoothAgentManagerClient;
using chromeos::BluetoothDeviceClient;
using chromeos::BluetoothGattCharacteristicClient;
using chromeos::BluetoothGattDescriptorClient;
using chromeos::BluetoothGattServiceClient;
using chromeos::BluetoothInputClient;
using chromeos::FakeBluetoothAdapterClient;
using chromeos::FakeBluetoothAgentManagerClient;
using chromeos::FakeBluetoothDeviceClient;
using chromeos::FakeBluetoothGattCharacteristicClient;
using chromeos::FakeBluetoothGattDescriptorClient;
using chromeos::FakeBluetoothGattServiceClient;
using chromeos::FakeBluetoothInputClient;
using chromeos::FakeDBusThreadManager;
using chromeos::PowerManagerClient;
using chromeos::STUB_DBUS_CLIENT_IMPLEMENTATION;

class ChromeOSMetricsProviderTest : public testing::Test {
 public:
  ChromeOSMetricsProviderTest() {}

 protected:
  virtual void SetUp() OVERRIDE {
#if defined(USE_X11)
    ui::DeviceDataManagerX11::CreateInstance();
#endif

    // Set up the fake Bluetooth environment,
    scoped_ptr<FakeDBusThreadManager> fake_dbus_thread_manager(
        new FakeDBusThreadManager);
    fake_dbus_thread_manager->SetBluetoothAdapterClient(
        scoped_ptr<BluetoothAdapterClient>(new FakeBluetoothAdapterClient));
    fake_dbus_thread_manager->SetBluetoothDeviceClient(
        scoped_ptr<BluetoothDeviceClient>(new FakeBluetoothDeviceClient));
    fake_dbus_thread_manager->SetBluetoothGattCharacteristicClient(
        scoped_ptr<BluetoothGattCharacteristicClient>(
            new FakeBluetoothGattCharacteristicClient));
    fake_dbus_thread_manager->SetBluetoothGattDescriptorClient(
        scoped_ptr<BluetoothGattDescriptorClient>(
            new FakeBluetoothGattDescriptorClient));
    fake_dbus_thread_manager->SetBluetoothGattServiceClient(
        scoped_ptr<BluetoothGattServiceClient>(
            new FakeBluetoothGattServiceClient));
    fake_dbus_thread_manager->SetBluetoothInputClient(
        scoped_ptr<BluetoothInputClient>(new FakeBluetoothInputClient));
    fake_dbus_thread_manager->SetBluetoothAgentManagerClient(
        scoped_ptr<BluetoothAgentManagerClient>(
            new FakeBluetoothAgentManagerClient));

    // Set up a PowerManagerClient instance for PerfProvider.
    fake_dbus_thread_manager->SetPowerManagerClient(
        scoped_ptr<PowerManagerClient>(
            PowerManagerClient::Create(STUB_DBUS_CLIENT_IMPLEMENTATION)));

    DBusThreadManager::InitializeForTesting(fake_dbus_thread_manager.release());

    // Grab pointers to members of the thread manager for easier testing.
    fake_bluetooth_adapter_client_ = static_cast<FakeBluetoothAdapterClient*>(
        DBusThreadManager::Get()->GetBluetoothAdapterClient());
    fake_bluetooth_device_client_ = static_cast<FakeBluetoothDeviceClient*>(
        DBusThreadManager::Get()->GetBluetoothDeviceClient());

    // Initialize the login state trackers.
    if (!chromeos::LoginState::IsInitialized())
      chromeos::LoginState::Initialize();
  }

  virtual void TearDown() OVERRIDE {
    // Destroy the login state tracker if it was initialized.
    chromeos::LoginState::Shutdown();

    DBusThreadManager::Shutdown();
  }

 protected:
  FakeBluetoothAdapterClient* fake_bluetooth_adapter_client_;
  FakeBluetoothDeviceClient* fake_bluetooth_device_client_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(ChromeOSMetricsProviderTest);
};

TEST_F(ChromeOSMetricsProviderTest, MultiProfileUserCount) {
  std::string user1("user1@example.com");
  std::string user2("user2@example.com");
  std::string user3("user3@example.com");

  // |scoped_enabler| takes over the lifetime of |user_manager|.
  chromeos::FakeUserManager* user_manager = new chromeos::FakeUserManager();
  chromeos::ScopedUserManagerEnabler scoped_enabler(user_manager);
  user_manager->AddKioskAppUser(user1);
  user_manager->AddKioskAppUser(user2);
  user_manager->AddKioskAppUser(user3);

  user_manager->LoginUser(user1);
  user_manager->LoginUser(user3);

  ChromeOSMetricsProvider provider;
  provider.OnDidCreateMetricsLog();
  metrics::SystemProfileProto system_profile;
  provider.ProvideSystemProfileMetrics(&system_profile);
  EXPECT_EQ(2u, system_profile.multi_profile_user_count());
}

TEST_F(ChromeOSMetricsProviderTest, MultiProfileCountInvalidated) {
  std::string user1("user1@example.com");
  std::string user2("user2@example.com");
  std::string user3("user3@example.com");

  // |scoped_enabler| takes over the lifetime of |user_manager|.
  chromeos::FakeUserManager* user_manager = new chromeos::FakeUserManager();
  chromeos::ScopedUserManagerEnabler scoped_enabler(user_manager);
  user_manager->AddKioskAppUser(user1);
  user_manager->AddKioskAppUser(user2);
  user_manager->AddKioskAppUser(user3);

  user_manager->LoginUser(user1);

  ChromeOSMetricsProvider provider;
  provider.OnDidCreateMetricsLog();

  metrics::SystemProfileProto system_profile;
  provider.ProvideSystemProfileMetrics(&system_profile);
  EXPECT_EQ(1u, system_profile.multi_profile_user_count());

  user_manager->LoginUser(user2);
  provider.ProvideSystemProfileMetrics(&system_profile);
  EXPECT_EQ(0u, system_profile.multi_profile_user_count());
}

TEST_F(ChromeOSMetricsProviderTest, BluetoothHardwareDisabled) {
  ChromeOSMetricsProvider provider;
  provider.OnDidCreateMetricsLog();
  metrics::SystemProfileProto system_profile;
  provider.ProvideSystemProfileMetrics(&system_profile);

  EXPECT_TRUE(system_profile.has_hardware());
  EXPECT_TRUE(system_profile.hardware().has_bluetooth());

  EXPECT_TRUE(system_profile.hardware().bluetooth().is_present());
  EXPECT_FALSE(system_profile.hardware().bluetooth().is_enabled());
}

TEST_F(ChromeOSMetricsProviderTest, BluetoothHardwareEnabled) {
  FakeBluetoothAdapterClient::Properties* properties =
      fake_bluetooth_adapter_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath));
  properties->powered.ReplaceValue(true);

  ChromeOSMetricsProvider provider;
  metrics::SystemProfileProto system_profile;
  provider.ProvideSystemProfileMetrics(&system_profile);

  EXPECT_TRUE(system_profile.has_hardware());
  EXPECT_TRUE(system_profile.hardware().has_bluetooth());

  EXPECT_TRUE(system_profile.hardware().bluetooth().is_present());
  EXPECT_TRUE(system_profile.hardware().bluetooth().is_enabled());
}

TEST_F(ChromeOSMetricsProviderTest, BluetoothPairedDevices) {
  // The fake bluetooth adapter class already claims to be paired with one
  // device when initialized. Add a second and third fake device to it so we
  // can test the cases where a device is not paired (LE device, generally)
  // and a device that does not have Device ID information.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kRequestPinCodePath));

  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kConfirmPasskeyPath));

  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kConfirmPasskeyPath));
  properties->paired.ReplaceValue(true);

  ChromeOSMetricsProvider provider;
  provider.OnDidCreateMetricsLog();
  metrics::SystemProfileProto system_profile;
  provider.ProvideSystemProfileMetrics(&system_profile);

  ASSERT_TRUE(system_profile.has_hardware());
  ASSERT_TRUE(system_profile.hardware().has_bluetooth());

  // Only two of the devices should appear.
  EXPECT_EQ(2, system_profile.hardware().bluetooth().paired_device_size());

  typedef metrics::SystemProfileProto::Hardware::Bluetooth::PairedDevice
      PairedDevice;

  // First device should match the Paired Device object, complete with
  // parsed Device ID information.
  PairedDevice device1 = system_profile.hardware().bluetooth().paired_device(0);

  EXPECT_EQ(FakeBluetoothDeviceClient::kPairedDeviceClass,
            device1.bluetooth_class());
  EXPECT_EQ(PairedDevice::DEVICE_COMPUTER, device1.type());
  EXPECT_EQ(0x001122U, device1.vendor_prefix());
  EXPECT_EQ(PairedDevice::VENDOR_ID_USB, device1.vendor_id_source());
  EXPECT_EQ(0x05ACU, device1.vendor_id());
  EXPECT_EQ(0x030DU, device1.product_id());
  EXPECT_EQ(0x0306U, device1.device_id());

  // Second device should match the Confirm Passkey object, this has
  // no Device ID information.
  PairedDevice device2 = system_profile.hardware().bluetooth().paired_device(1);

  EXPECT_EQ(FakeBluetoothDeviceClient::kConfirmPasskeyClass,
            device2.bluetooth_class());
  EXPECT_EQ(PairedDevice::DEVICE_PHONE, device2.type());
  EXPECT_EQ(0x207D74U, device2.vendor_prefix());
  EXPECT_EQ(PairedDevice::VENDOR_ID_UNKNOWN, device2.vendor_id_source());
}

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "chromeos/dbus/bluetooth_profile_service_provider.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_bluetooth_adapter_client.h"
#include "chromeos/dbus/fake_bluetooth_agent_manager_client.h"
#include "chromeos/dbus/fake_bluetooth_device_client.h"
#include "chromeos/dbus/fake_bluetooth_profile_manager_client.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_chromeos.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_adapter_profile_chromeos.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "testing/gtest/include/gtest/gtest.h"

using device::BluetoothAdapter;
using device::BluetoothUUID;

namespace {

void DoNothingDBusErrorCallback(const std::string& error_name,
                                const std::string& error_message) {
}

}  // namespace

namespace chromeos {

class BluetoothAdapterProfileChromeOSTest : public testing::Test {
 public:
  BluetoothAdapterProfileChromeOSTest()
      : fake_delegate_paired_(FakeBluetoothDeviceClient::kPairedDevicePath),
        fake_delegate_autopair_(FakeBluetoothDeviceClient::kLegacyAutopairPath),
        fake_delegate_listen_(""),
        profile_(nullptr),
        success_callback_count_(0),
        error_callback_count_(0) {}

  void SetUp() override {
    scoped_ptr<DBusThreadManagerSetter> dbus_setter =
        DBusThreadManager::GetSetterForTesting();

    dbus_setter->SetBluetoothAdapterClient(
        scoped_ptr<BluetoothAdapterClient>(new FakeBluetoothAdapterClient));
    dbus_setter->SetBluetoothAgentManagerClient(
        scoped_ptr<BluetoothAgentManagerClient>(
            new FakeBluetoothAgentManagerClient));
    dbus_setter->SetBluetoothDeviceClient(
        scoped_ptr<BluetoothDeviceClient>(new FakeBluetoothDeviceClient));
    dbus_setter->SetBluetoothProfileManagerClient(
        scoped_ptr<BluetoothProfileManagerClient>(
            new FakeBluetoothProfileManagerClient));

    // Grab a pointer to the adapter.
    device::BluetoothAdapterFactory::GetAdapter(
        base::Bind(&BluetoothAdapterProfileChromeOSTest::AdapterCallback,
                   base::Unretained(this)));
    ASSERT_TRUE(adapter_.get() != nullptr);
    ASSERT_TRUE(adapter_->IsInitialized());
    ASSERT_TRUE(adapter_->IsPresent());

    // Turn on the adapter.
    adapter_->SetPowered(true, base::Bind(&base::DoNothing),
                         base::Bind(&base::DoNothing));
    ASSERT_TRUE(adapter_->IsPowered());
  }

  void TearDown() override {
    adapter_ = nullptr;
    DBusThreadManager::Shutdown();
  }

  void AdapterCallback(scoped_refptr<BluetoothAdapter> adapter) {
    adapter_ = adapter;
  }

  class FakeDelegate
      : public chromeos::BluetoothProfileServiceProvider::Delegate {
   public:
    FakeDelegate(std::string device_path) : connections_(0) {
      device_path_ = dbus::ObjectPath(device_path);
    }

    // BluetoothProfileServiceProvider::Delegate:
    void Released() override {
      // noop
    }

    void NewConnection(
        const dbus::ObjectPath& device_path,
        scoped_ptr<dbus::FileDescriptor> fd,
        const BluetoothProfileServiceProvider::Delegate::Options& options,
        const ConfirmationCallback& callback) override {
      VLOG(1) << "connection for " << device_path.value() << " on "
              << device_path_.value();
      ++connections_;
      fd->CheckValidity();
      close(fd->TakeValue());
      callback.Run(SUCCESS);
      if (device_path_.value() != "")
        ASSERT_TRUE(device_path == device_path_);
    }

    void RequestDisconnection(const dbus::ObjectPath& device_path,
                              const ConfirmationCallback& callback) override {
      VLOG(1) << "disconnect " << device_path.value();
      ++disconnections_;
    }

    void Cancel() override {
      VLOG(1) << "cancel";
      // noop
    }

    unsigned int connections_;
    unsigned int disconnections_;
    dbus::ObjectPath device_path_;
  };

  FakeDelegate fake_delegate_paired_;
  FakeDelegate fake_delegate_autopair_;
  FakeDelegate fake_delegate_listen_;

  BluetoothAdapterProfileChromeOS* profile_;

  void DBusConnectSuccessCallback() { ++success_callback_count_; }

  void DBusErrorCallback(const std::string& error_name,
                         const std::string& error_message) {
    VLOG(1) << "DBus Connect Error: " << error_name << " - " << error_message;
    ++error_callback_count_;
  }

 protected:
  base::MessageLoop message_loop_;

  scoped_refptr<BluetoothAdapter> adapter_;

  unsigned int success_callback_count_;
  unsigned int error_callback_count_;
};

TEST_F(BluetoothAdapterProfileChromeOSTest, DelegateCount) {
  BluetoothUUID uuid(FakeBluetoothProfileManagerClient::kRfcommUuid);
  BluetoothProfileManagerClient::Options options;

  options.require_authentication.reset(new bool(false));

  profile_ = BluetoothAdapterProfileChromeOS::Register(
      static_cast<BluetoothAdapterChromeOS*>(adapter_.get()), uuid, options,
      base::Bind(&base::DoNothing), base::Bind(&DoNothingDBusErrorCallback));

  message_loop_.RunUntilIdle();

  EXPECT_TRUE(profile_);

  EXPECT_EQ(0U, profile_->DelegateCount());

  profile_->SetDelegate(fake_delegate_paired_.device_path_,
                        &fake_delegate_paired_);

  EXPECT_EQ(1U, profile_->DelegateCount());

  profile_->RemoveDelegate(fake_delegate_autopair_.device_path_,
                           base::Bind(&base::DoNothing));

  EXPECT_EQ(1U, profile_->DelegateCount());

  profile_->RemoveDelegate(fake_delegate_paired_.device_path_,
                           base::Bind(&base::DoNothing));

  EXPECT_EQ(0U, profile_->DelegateCount());

  delete profile_;
};

TEST_F(BluetoothAdapterProfileChromeOSTest, BlackHole) {
  BluetoothUUID uuid(FakeBluetoothProfileManagerClient::kRfcommUuid);
  BluetoothProfileManagerClient::Options options;

  options.require_authentication.reset(new bool(false));

  profile_ = BluetoothAdapterProfileChromeOS::Register(
      static_cast<BluetoothAdapterChromeOS*>(adapter_.get()), uuid, options,
      base::Bind(
          &BluetoothAdapterProfileChromeOSTest::DBusConnectSuccessCallback,
          base::Unretained(this)),
      base::Bind(&BluetoothAdapterProfileChromeOSTest::DBusErrorCallback,
                 base::Unretained(this)));

  message_loop_.RunUntilIdle();

  EXPECT_TRUE(profile_);
  EXPECT_EQ(1U, success_callback_count_);
  EXPECT_EQ(0U, error_callback_count_);

  DBusThreadManager::Get()->GetBluetoothDeviceClient()->ConnectProfile(
      dbus::ObjectPath(FakeBluetoothDeviceClient::kPairedDevicePath),
      FakeBluetoothProfileManagerClient::kRfcommUuid,
      base::Bind(
          &BluetoothAdapterProfileChromeOSTest::DBusConnectSuccessCallback,
          base::Unretained(this)),
      base::Bind(&BluetoothAdapterProfileChromeOSTest::DBusErrorCallback,
                 base::Unretained(this)));

  message_loop_.RunUntilIdle();

  EXPECT_EQ(1U, success_callback_count_);
  EXPECT_EQ(1U, error_callback_count_);

  EXPECT_EQ(0U, fake_delegate_paired_.connections_);

  delete profile_;
};

TEST_F(BluetoothAdapterProfileChromeOSTest, Routing) {
  BluetoothUUID uuid(FakeBluetoothProfileManagerClient::kRfcommUuid);
  BluetoothProfileManagerClient::Options options;

  options.require_authentication.reset(new bool(false));

  profile_ = BluetoothAdapterProfileChromeOS::Register(
      static_cast<BluetoothAdapterChromeOS*>(adapter_.get()), uuid, options,
      base::Bind(
          &BluetoothAdapterProfileChromeOSTest::DBusConnectSuccessCallback,
          base::Unretained(this)),
      base::Bind(&BluetoothAdapterProfileChromeOSTest::DBusErrorCallback,
                 base::Unretained(this)));

  message_loop_.RunUntilIdle();

  ASSERT_TRUE(profile_);

  profile_->SetDelegate(fake_delegate_paired_.device_path_,
                        &fake_delegate_paired_);
  profile_->SetDelegate(fake_delegate_autopair_.device_path_,
                        &fake_delegate_autopair_);
  profile_->SetDelegate(fake_delegate_listen_.device_path_,
                        &fake_delegate_listen_);

  DBusThreadManager::Get()->GetBluetoothDeviceClient()->ConnectProfile(
      dbus::ObjectPath(FakeBluetoothDeviceClient::kPairedDevicePath),
      FakeBluetoothProfileManagerClient::kRfcommUuid,
      base::Bind(
          &BluetoothAdapterProfileChromeOSTest::DBusConnectSuccessCallback,
          base::Unretained(this)),
      base::Bind(&BluetoothAdapterProfileChromeOSTest::DBusErrorCallback,
                 base::Unretained(this)));

  message_loop_.RunUntilIdle();

  EXPECT_EQ(2U, success_callback_count_);
  EXPECT_EQ(0U, error_callback_count_);

  EXPECT_EQ(1U, fake_delegate_paired_.connections_);

  DBusThreadManager::Get()->GetBluetoothDeviceClient()->ConnectProfile(
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLegacyAutopairPath),
      FakeBluetoothProfileManagerClient::kRfcommUuid,
      base::Bind(
          &BluetoothAdapterProfileChromeOSTest::DBusConnectSuccessCallback,
          base::Unretained(this)),
      base::Bind(&BluetoothAdapterProfileChromeOSTest::DBusErrorCallback,
                 base::Unretained(this)));

  message_loop_.RunUntilIdle();

  EXPECT_EQ(3U, success_callback_count_);
  EXPECT_EQ(0U, error_callback_count_);

  EXPECT_EQ(1U, fake_delegate_autopair_.connections_);

  // Incoming connections look the same from BlueZ.
  DBusThreadManager::Get()->GetBluetoothDeviceClient()->ConnectProfile(
      dbus::ObjectPath(FakeBluetoothDeviceClient::kDisplayPinCodePath),
      FakeBluetoothProfileManagerClient::kRfcommUuid,
      base::Bind(
          &BluetoothAdapterProfileChromeOSTest::DBusConnectSuccessCallback,
          base::Unretained(this)),
      base::Bind(&BluetoothAdapterProfileChromeOSTest::DBusErrorCallback,
                 base::Unretained(this)));

  message_loop_.RunUntilIdle();

  EXPECT_EQ(4U, success_callback_count_);
  EXPECT_EQ(0U, error_callback_count_);

  EXPECT_EQ(1U, fake_delegate_listen_.connections_);

  delete profile_;
};

}  // namespace chromeos

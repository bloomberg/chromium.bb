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

namespace chromeos {

class BluetoothAdapterProfileChromeOSTest : public testing::Test {
 public:
  BluetoothAdapterProfileChromeOSTest()
      : success_callback_count_(0),
        error_callback_count_(0),
        fake_delegate_paired_(FakeBluetoothDeviceClient::kPairedDevicePath),
        fake_delegate_autopair_(FakeBluetoothDeviceClient::kLegacyAutopairPath),
        fake_delegate_listen_(""),
        profile_user_ptr_(nullptr) {}

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
    profile_.reset();
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
      ++connections_;
      fd->CheckValidity();
      close(fd->TakeValue());
      callback.Run(SUCCESS);
      if (device_path_.value() != "")
        ASSERT_EQ(device_path_, device_path);
    }

    void RequestDisconnection(const dbus::ObjectPath& device_path,
                              const ConfirmationCallback& callback) override {
      ++disconnections_;
    }

    void Cancel() override {
      // noop
    }

    unsigned int connections_;
    unsigned int disconnections_;
    dbus::ObjectPath device_path_;
  };

  void ProfileSuccessCallback(
      scoped_ptr<BluetoothAdapterProfileChromeOS> profile) {
    profile_.swap(profile);
    ++success_callback_count_;
  }

  void ProfileUserSuccessCallback(BluetoothAdapterProfileChromeOS* profile) {
    profile_user_ptr_ = profile;
    ++success_callback_count_;
  }

  void MatchedProfileCallback(BluetoothAdapterProfileChromeOS* profile) {
    ASSERT_EQ(profile_user_ptr_, profile);
    ++success_callback_count_;
  }

  void DBusConnectSuccessCallback() { ++success_callback_count_; }

  void DBusErrorCallback(const std::string& error_name,
                         const std::string& error_message) {
    ++error_callback_count_;
  }

  void BasicErrorCallback(const std::string& error_message) {
    ++error_callback_count_;
  }

 protected:
  base::MessageLoop message_loop_;

  scoped_refptr<BluetoothAdapter> adapter_;

  unsigned int success_callback_count_;
  unsigned int error_callback_count_;

  FakeDelegate fake_delegate_paired_;
  FakeDelegate fake_delegate_autopair_;
  FakeDelegate fake_delegate_listen_;

  scoped_ptr<BluetoothAdapterProfileChromeOS> profile_;

  // unowned pointer as expected to be used by clients of
  // BluetoothAdapterChromeOS::UseProfile like BluetoothSocketChromeOS
  BluetoothAdapterProfileChromeOS* profile_user_ptr_;
};

TEST_F(BluetoothAdapterProfileChromeOSTest, DelegateCount) {
  BluetoothUUID uuid(FakeBluetoothProfileManagerClient::kRfcommUuid);
  BluetoothProfileManagerClient::Options options;

  options.require_authentication.reset(new bool(false));

  BluetoothAdapterProfileChromeOS::Register(
      uuid, options,
      base::Bind(&BluetoothAdapterProfileChromeOSTest::ProfileSuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAdapterProfileChromeOSTest::DBusErrorCallback,
                 base::Unretained(this)));

  message_loop_.RunUntilIdle();

  EXPECT_TRUE(profile_);
  EXPECT_EQ(1U, success_callback_count_);
  EXPECT_EQ(0U, error_callback_count_);

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
};

TEST_F(BluetoothAdapterProfileChromeOSTest, BlackHole) {
  BluetoothUUID uuid(FakeBluetoothProfileManagerClient::kRfcommUuid);
  BluetoothProfileManagerClient::Options options;

  options.require_authentication.reset(new bool(false));

  BluetoothAdapterProfileChromeOS::Register(
      uuid, options,
      base::Bind(&BluetoothAdapterProfileChromeOSTest::ProfileSuccessCallback,
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
};

TEST_F(BluetoothAdapterProfileChromeOSTest, Routing) {
  BluetoothUUID uuid(FakeBluetoothProfileManagerClient::kRfcommUuid);
  BluetoothProfileManagerClient::Options options;

  options.require_authentication.reset(new bool(false));

  BluetoothAdapterProfileChromeOS::Register(
      uuid, options,
      base::Bind(&BluetoothAdapterProfileChromeOSTest::ProfileSuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAdapterProfileChromeOSTest::DBusErrorCallback,
                 base::Unretained(this)));

  message_loop_.RunUntilIdle();

  ASSERT_TRUE(profile_);
  ASSERT_EQ(1U, success_callback_count_);
  ASSERT_EQ(0U, error_callback_count_);

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
};

TEST_F(BluetoothAdapterProfileChromeOSTest, SimultaneousRegister) {
  BluetoothUUID uuid(FakeBluetoothProfileManagerClient::kRfcommUuid);
  BluetoothProfileManagerClient::Options options;
  BluetoothAdapterChromeOS* adapter =
      static_cast<BluetoothAdapterChromeOS*>(adapter_.get());

  options.require_authentication.reset(new bool(false));

  success_callback_count_ = 0;
  error_callback_count_ = 0;

  adapter->UseProfile(
      uuid, fake_delegate_paired_.device_path_, options, &fake_delegate_paired_,
      base::Bind(
          &BluetoothAdapterProfileChromeOSTest::ProfileUserSuccessCallback,
          base::Unretained(this)),
      base::Bind(&BluetoothAdapterProfileChromeOSTest::BasicErrorCallback,
                 base::Unretained(this)));

  adapter->UseProfile(
      uuid, fake_delegate_autopair_.device_path_, options,
      &fake_delegate_autopair_,
      base::Bind(&BluetoothAdapterProfileChromeOSTest::MatchedProfileCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAdapterProfileChromeOSTest::BasicErrorCallback,
                 base::Unretained(this)));

  message_loop_.RunUntilIdle();

  EXPECT_TRUE(profile_user_ptr_);
  EXPECT_EQ(2U, success_callback_count_);
  EXPECT_EQ(0U, error_callback_count_);

  adapter->ReleaseProfile(fake_delegate_paired_.device_path_,
                          profile_user_ptr_);
  adapter->ReleaseProfile(fake_delegate_autopair_.device_path_,
                          profile_user_ptr_);

  message_loop_.RunUntilIdle();
};

TEST_F(BluetoothAdapterProfileChromeOSTest, SimultaneousRegisterFail) {
  BluetoothUUID uuid(FakeBluetoothProfileManagerClient::kUnregisterableUuid);
  BluetoothProfileManagerClient::Options options;
  BluetoothAdapterChromeOS* adapter =
      static_cast<BluetoothAdapterChromeOS*>(adapter_.get());

  options.require_authentication.reset(new bool(false));

  success_callback_count_ = 0;
  error_callback_count_ = 0;

  adapter->UseProfile(
      uuid, fake_delegate_paired_.device_path_, options, &fake_delegate_paired_,
      base::Bind(
          &BluetoothAdapterProfileChromeOSTest::ProfileUserSuccessCallback,
          base::Unretained(this)),
      base::Bind(&BluetoothAdapterProfileChromeOSTest::BasicErrorCallback,
                 base::Unretained(this)));

  adapter->UseProfile(
      uuid, fake_delegate_autopair_.device_path_, options,
      &fake_delegate_autopair_,
      base::Bind(&BluetoothAdapterProfileChromeOSTest::MatchedProfileCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAdapterProfileChromeOSTest::BasicErrorCallback,
                 base::Unretained(this)));

  message_loop_.RunUntilIdle();

  EXPECT_FALSE(profile_user_ptr_);
  EXPECT_EQ(0U, success_callback_count_);
  EXPECT_EQ(2U, error_callback_count_);
};

}  // namespace chromeos

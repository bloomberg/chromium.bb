// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "chromeos/dbus/fake_bluetooth_adapter_client.h"
#include "chromeos/dbus/fake_bluetooth_agent_manager_client.h"
#include "chromeos/dbus/fake_bluetooth_device_client.h"
#include "chromeos/dbus/fake_bluetooth_input_client.h"
#include "chromeos/dbus/fake_bluetooth_profile_manager_client.h"
#include "chromeos/dbus/fake_bluetooth_profile_service_provider.h"
#include "chromeos/dbus/fake_dbus_thread_manager.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_chromeos.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_device_chromeos.h"
#include "device/bluetooth/bluetooth_profile.h"
#include "device/bluetooth/bluetooth_profile_chromeos.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/bluetooth_socket_chromeos.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "net/base/io_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

using device::BluetoothAdapter;
using device::BluetoothDevice;
using device::BluetoothProfile;
using device::BluetoothSocket;
using device::BluetoothUUID;

namespace chromeos {

class BluetoothProfileChromeOSTest : public testing::Test {
 public:
  BluetoothProfileChromeOSTest()
      : callback_count_(0),
        error_callback_count_(0),
        profile_callback_count_(0),
        connection_callback_count_(0),
        last_profile_(NULL),
        last_device_(NULL) {}

  virtual void SetUp() {
    FakeDBusThreadManager* fake_dbus_thread_manager = new FakeDBusThreadManager;
    fake_bluetooth_profile_manager_client_ =
        new FakeBluetoothProfileManagerClient;
    fake_dbus_thread_manager->SetBluetoothProfileManagerClient(
        scoped_ptr<BluetoothProfileManagerClient>(
            fake_bluetooth_profile_manager_client_));
    fake_dbus_thread_manager->SetBluetoothAgentManagerClient(
        scoped_ptr<BluetoothAgentManagerClient>(
            new FakeBluetoothAgentManagerClient));
    fake_dbus_thread_manager->SetBluetoothAdapterClient(
        scoped_ptr<BluetoothAdapterClient>(new FakeBluetoothAdapterClient));
    fake_dbus_thread_manager->SetBluetoothDeviceClient(
        scoped_ptr<BluetoothDeviceClient>(new FakeBluetoothDeviceClient));
    fake_dbus_thread_manager->SetBluetoothInputClient(
        scoped_ptr<BluetoothInputClient>(new FakeBluetoothInputClient));
    DBusThreadManager::InitializeForTesting(fake_dbus_thread_manager);

    device::BluetoothAdapterFactory::GetAdapter(
        base::Bind(&BluetoothProfileChromeOSTest::AdapterCallback,
                   base::Unretained(this)));
    ASSERT_TRUE(adapter_.get() != NULL);
    ASSERT_TRUE(adapter_->IsInitialized());
    ASSERT_TRUE(adapter_->IsPresent());

    adapter_->SetPowered(
        true,
        base::Bind(&base::DoNothing),
        base::Bind(&base::DoNothing));
    ASSERT_TRUE(adapter_->IsPowered());
  }

  virtual void TearDown() {
    adapter_ = NULL;
    DBusThreadManager::Shutdown();
  }

  void AdapterCallback(scoped_refptr<BluetoothAdapter> adapter) {
    adapter_ = adapter;
  }

  void Callback() {
    ++callback_count_;
  }

  void ErrorCallback() {
    ++error_callback_count_;

    message_loop_.Quit();
  }

  void ConnectToProfileErrorCallback(const std::string& error) {
    ErrorCallback();
  }

  void ProfileCallback(BluetoothProfile* profile) {
    ++profile_callback_count_;
    last_profile_ = profile;
  }

  void ConnectionCallback(const BluetoothDevice *device,
                          scoped_refptr<BluetoothSocket> socket) {
    ++connection_callback_count_;
    last_device_ = device;
    last_socket_ = socket;

    message_loop_.Quit();
  }

 protected:
  base::MessageLoopForIO message_loop_;

  FakeBluetoothProfileManagerClient* fake_bluetooth_profile_manager_client_;
  scoped_refptr<BluetoothAdapter> adapter_;

  unsigned int callback_count_;
  unsigned int error_callback_count_;
  unsigned int profile_callback_count_;
  unsigned int connection_callback_count_;
  BluetoothProfile* last_profile_;
  const BluetoothDevice* last_device_;
  scoped_refptr<BluetoothSocket> last_socket_;
};

TEST_F(BluetoothProfileChromeOSTest, L2capEndToEnd) {
  // TODO(rpaquay): Implement this test once the ChromeOS implementation of
  // BluetoothSocket is done.
#if 0
  // Register the profile and expect the profile object to be passed to the
  // callback.
  BluetoothProfile::Options options;
  BluetoothProfile::Register(
      BluetoothUUID(FakeBluetoothProfileManagerClient::kL2capUuid),
      options,
      base::Bind(&BluetoothProfileChromeOSTest::ProfileCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1U, profile_callback_count_);
  EXPECT_TRUE(last_profile_ != NULL);
  BluetoothProfile* profile = last_profile_;

  // Make sure we have a profile service provider for it.
  FakeBluetoothProfileServiceProvider* profile_service_provider =
      fake_bluetooth_profile_manager_client_->GetProfileServiceProvider(
          FakeBluetoothProfileManagerClient::kL2capUuid);
  EXPECT_TRUE(profile_service_provider != NULL);

  // Register the connection callback.
  profile->SetConnectionCallback(
      base::Bind(&BluetoothProfileChromeOSTest::ConnectionCallback,
                 base::Unretained(this)));

  // Connect to the device, expect the success callback to be called and the
  // connection callback to be called with the device we passed and a new
  // socket instance.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_TRUE(device != NULL);

  device->ConnectToProfile(
      profile,
      base::Bind(&BluetoothProfileChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothProfileChromeOSTest::ConnectToProfileErrorCallback,
                 base::Unretained(this)));

  message_loop_.Run();

  EXPECT_EQ(1U, callback_count_);
  EXPECT_EQ(0U, error_callback_count_);

  EXPECT_EQ(1U, connection_callback_count_);
  EXPECT_EQ(device, last_device_);
  EXPECT_TRUE(last_socket_.get() != NULL);

  // Take the ownership of the socket for the remainder of the test and set
  // up buffers for read/write tests.
  scoped_refptr<BluetoothSocket> socket = last_socket_;
  last_socket_ = NULL;

  bool success;
  scoped_refptr<net::GrowableIOBuffer> read_buffer;

  scoped_refptr<net::StringIOBuffer> base_buffer(
      new net::StringIOBuffer("test"));
  scoped_refptr<net::DrainableIOBuffer> write_buffer;

  // Read data from the socket; since no data should be waiting, this should
  // return success but no data.
  read_buffer = new net::GrowableIOBuffer;
  success = socket->Receive(read_buffer.get());
  EXPECT_TRUE(success);
  EXPECT_EQ(0, read_buffer->capacity());
  EXPECT_EQ(0, read_buffer->offset());
  EXPECT_EQ("", socket->GetLastErrorMessage());

  // Write data to the socket; the data should be consumed and no bytes should
  // be remaining.
  write_buffer =
      new net::DrainableIOBuffer(base_buffer.get(), base_buffer->size());
  success = socket->Send(write_buffer.get());
  EXPECT_TRUE(success);
  EXPECT_EQ(base_buffer->size(), write_buffer->BytesConsumed());
  EXPECT_EQ(0, write_buffer->BytesRemaining());
  EXPECT_EQ("", socket->GetLastErrorMessage());

  // Read data from the socket; this should match the data we sent since the
  // server just echoes us. We have to spin here until there is actually data
  // to read.
  read_buffer = new net::GrowableIOBuffer;
  do {
    success = socket->Receive(read_buffer.get());
  } while (success && read_buffer->offset() == 0);
  EXPECT_TRUE(success);
  EXPECT_NE(0, read_buffer->capacity());
  EXPECT_EQ(base_buffer->size(), read_buffer->offset());
  EXPECT_EQ("", socket->GetLastErrorMessage());

  std::string data = std::string(read_buffer->StartOfBuffer(),
                                 read_buffer->offset());
  EXPECT_EQ("test", data);

  // Write data to the socket; since the socket is closed, this should return
  // an error without writing the data and "Disconnected" as the message.
  write_buffer =
      new net::DrainableIOBuffer(base_buffer.get(), base_buffer->size());
  success = socket->Send(write_buffer.get());
  EXPECT_FALSE(success);
  EXPECT_EQ(0, write_buffer->BytesConsumed());
  EXPECT_EQ(base_buffer->size(), write_buffer->BytesRemaining());
  EXPECT_EQ("Disconnected", socket->GetLastErrorMessage());

  // Read data from the socket; since the socket is closed, this should return
  // an error with "Disconnected" as the last message.
  read_buffer = new net::GrowableIOBuffer;
  success = socket->Receive(read_buffer.get());
  EXPECT_FALSE(success);
  EXPECT_EQ(0, read_buffer->capacity());
  EXPECT_EQ(0, read_buffer->offset());
  EXPECT_EQ("Disconnected", socket->GetLastErrorMessage());

  // Close our end of the socket.
  socket = NULL;

  // Unregister the profile, make sure it's no longer registered.
  last_profile_->Unregister();

  profile_service_provider =
      fake_bluetooth_profile_manager_client_->GetProfileServiceProvider(
          FakeBluetoothProfileManagerClient::kL2capUuid);
  EXPECT_TRUE(profile_service_provider == NULL);
#endif
}

TEST_F(BluetoothProfileChromeOSTest, RfcommEndToEnd) {
  // TODO(rpaquay): Implement this test once the ChromeOS implementation of
  // BluetoothSocket is done.
#if 0
  // Register the profile and expect the profile object to be passed to the
  // callback.
  BluetoothProfile::Options options;
  BluetoothProfile::Register(
      BluetoothUUID(FakeBluetoothProfileManagerClient::kRfcommUuid),
      options,
      base::Bind(&BluetoothProfileChromeOSTest::ProfileCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1U, profile_callback_count_);
  EXPECT_TRUE(last_profile_ != NULL);
  BluetoothProfile* profile = last_profile_;

  // Make sure we have a profile service provider for it.
  FakeBluetoothProfileServiceProvider* profile_service_provider =
      fake_bluetooth_profile_manager_client_->GetProfileServiceProvider(
          FakeBluetoothProfileManagerClient::kRfcommUuid);
  EXPECT_TRUE(profile_service_provider != NULL);

  // Register the connection callback.
  profile->SetConnectionCallback(
      base::Bind(&BluetoothProfileChromeOSTest::ConnectionCallback,
                 base::Unretained(this)));

  // Connect to the device, expect the success callback to be called and the
  // connection callback to be called with the device we passed and a new
  // socket instance.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_TRUE(device != NULL);

  device->ConnectToProfile(
      profile,
      base::Bind(&BluetoothProfileChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothProfileChromeOSTest::ConnectToProfileErrorCallback,
                 base::Unretained(this)));

  message_loop_.Run();

  EXPECT_EQ(1U, callback_count_);
  EXPECT_EQ(0U, error_callback_count_);

  EXPECT_EQ(1U, connection_callback_count_);
  EXPECT_EQ(device, last_device_);
  EXPECT_TRUE(last_socket_.get() != NULL);

  // Take the ownership of the socket for the remainder of the test and set
  // up buffers for read/write tests.
  scoped_refptr<BluetoothSocket> socket = last_socket_;
  last_socket_ = NULL;

  bool success;
  scoped_refptr<net::GrowableIOBuffer> read_buffer;

  scoped_refptr<net::StringIOBuffer> base_buffer(
      new net::StringIOBuffer("test"));
  scoped_refptr<net::DrainableIOBuffer> write_buffer;

  // Read data from the socket; since no data should be waiting, this should
  // return success but no data.
  read_buffer = new net::GrowableIOBuffer;
  success = socket->Receive(read_buffer.get());
  EXPECT_TRUE(success);
  EXPECT_EQ(0, read_buffer->offset());
  EXPECT_EQ("", socket->GetLastErrorMessage());

  // Write data to the socket; the data should be consumed and no bytes should
  // be remaining.
  write_buffer =
      new net::DrainableIOBuffer(base_buffer.get(), base_buffer->size());
  success = socket->Send(write_buffer.get());
  EXPECT_TRUE(success);
  EXPECT_EQ(base_buffer->size(), write_buffer->BytesConsumed());
  EXPECT_EQ(0, write_buffer->BytesRemaining());
  EXPECT_EQ("", socket->GetLastErrorMessage());

  // Read data from the socket; this should match the data we sent since the
  // server just echoes us. We have to spin here until there is actually data
  // to read.
  read_buffer = new net::GrowableIOBuffer;
  do {
    success = socket->Receive(read_buffer.get());
  } while (success && read_buffer->offset() == 0);
  EXPECT_TRUE(success);
  EXPECT_NE(0, read_buffer->capacity());
  EXPECT_EQ(base_buffer->size(), read_buffer->offset());
  EXPECT_EQ("", socket->GetLastErrorMessage());

  std::string data = std::string(read_buffer->StartOfBuffer(),
                                 read_buffer->offset());
  EXPECT_EQ("test", data);

  // Write data to the socket; since the socket is closed, this should return
  // an error without writing the data and "Disconnected" as the message.
  write_buffer =
      new net::DrainableIOBuffer(base_buffer.get(), base_buffer->size());
  success = socket->Send(write_buffer.get());
  EXPECT_FALSE(success);
  EXPECT_EQ(0, write_buffer->BytesConsumed());
  EXPECT_EQ(base_buffer->size(), write_buffer->BytesRemaining());
  EXPECT_EQ("Disconnected", socket->GetLastErrorMessage());

  // Read data from the socket; since the socket is closed, this should return
  // an error with "Disconnected" as the last message.
  read_buffer = new net::GrowableIOBuffer;
  success = socket->Receive(read_buffer.get());
  EXPECT_FALSE(success);
  EXPECT_EQ(0, read_buffer->offset());
  EXPECT_EQ("Disconnected", socket->GetLastErrorMessage());

  // Close our end of the socket.
  socket = NULL;

  // Unregister the profile, make sure it's no longer registered.
  last_profile_->Unregister();

  profile_service_provider =
      fake_bluetooth_profile_manager_client_->GetProfileServiceProvider(
          FakeBluetoothProfileManagerClient::kRfcommUuid);
  EXPECT_TRUE(profile_service_provider == NULL);
#endif
}

}  // namespace chromeos

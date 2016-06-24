// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/bluetooth_service_record_bluez.h"

#include <memory>
#include <string>

#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluez/bluetooth_adapter_bluez.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bluez {

namespace {

constexpr uint16_t kServiceUuidAttributeId = 0x0003;

constexpr char kServiceUuid1[] = "00001801-0000-1000-8000-00805f9b34fb";
constexpr char kServiceUuid2[] = "00001801-0000-1000-7000-00805f9b34fb";
constexpr char kServiceUuid3[] = "00001801-0000-1000-3000-00805f9b34fb";

}  // namespace

class BluetoothServiceRecordBlueZTest : public testing::Test {
 public:
  BluetoothServiceRecordBlueZTest()
      : adapter_bluez_(nullptr),
        success_callbacks_(0),
        error_callbacks_(0),
        run_loop_(nullptr),
        last_seen_handle_(0) {}

  void SetUp() override {
    std::unique_ptr<bluez::BluezDBusManagerSetter> dbus_setter =
        bluez::BluezDBusManager::GetSetterForTesting();

    device::BluetoothAdapterFactory::GetAdapter(
        base::Bind(&BluetoothServiceRecordBlueZTest::AdapterCallback,
                   base::Unretained(this)));
    RunRunLoop();
  }

  void TearDown() override {
    device::BluetoothAdapterFactory::Shutdown();
    bluez::BluezDBusManager::Shutdown();
  }

  uint32_t CreateServiceRecordWithCallbacks(
      const BluetoothServiceRecordBlueZ& record) {
    const size_t old_success_callbacks = success_callbacks_;
    const size_t old_error_callbacks = error_callbacks_;
    // Note: Our fake implementation will never give us a handle of 0, so we
    // can know that we will never have a record with the handle 0. Hence using
    // 0 as an invalid handle return value is okay.
    last_seen_handle_ = 0;
    adapter_bluez_->CreateServiceRecord(
        record,
        base::Bind(
            &BluetoothServiceRecordBlueZTest::CreateServiceSuccessCallback,
            base::Unretained(this)),
        base::Bind(&BluetoothServiceRecordBlueZTest::ErrorCallback,
                   base::Unretained(this)));
    EXPECT_EQ(old_success_callbacks + 1, success_callbacks_);
    EXPECT_EQ(old_error_callbacks, error_callbacks_);
    EXPECT_NE(0u, last_seen_handle_);
    return last_seen_handle_;
  }

  void RemoveServiceRecordWithCallbacks(uint32_t handle, bool expect_success) {
    const size_t old_success_callbacks = success_callbacks_;
    const size_t old_error_callbacks = error_callbacks_;
    adapter_bluez_->RemoveServiceRecord(
        handle,
        base::Bind(
            &BluetoothServiceRecordBlueZTest::RemoveServiceSuccessCallback,
            base::Unretained(this)),
        base::Bind(&BluetoothServiceRecordBlueZTest::ErrorCallback,
                   base::Unretained(this)));
    size_t success = expect_success ? 1 : 0;
    EXPECT_EQ(old_success_callbacks + success, success_callbacks_);
    EXPECT_EQ(old_error_callbacks + 1 - success, error_callbacks_);
  }

 protected:
  BluetoothServiceRecordBlueZ CreateaServiceRecord(const std::string uuid) {
    std::map<uint16_t, BluetoothServiceAttributeValueBlueZ> attributes;
    attributes.insert(std::pair<uint16_t, BluetoothServiceAttributeValueBlueZ>(
        kServiceUuidAttributeId,
        BluetoothServiceAttributeValueBlueZ(
            BluetoothServiceAttributeValueBlueZ::UUID, 16,
            base::MakeUnique<base::StringValue>(uuid))));
    return BluetoothServiceRecordBlueZ(attributes);
  }

  scoped_refptr<device::BluetoothAdapter> adapter_;
  BluetoothAdapterBlueZ* adapter_bluez_;
  size_t success_callbacks_;
  size_t error_callbacks_;

 private:
  void RunRunLoop() {
    run_loop_ = base::MakeUnique<base::RunLoop>();
    run_loop_->Run();
  }

  void QuitRunLoop() {
    if (run_loop_)
      run_loop_->Quit();
  }

  void AdapterCallback(scoped_refptr<device::BluetoothAdapter> adapter) {
    adapter_ = adapter;
    adapter_bluez_ = static_cast<BluetoothAdapterBlueZ*>(adapter_.get());
    QuitRunLoop();
  }

  void CreateServiceSuccessCallback(uint32_t handle) {
    last_seen_handle_ = handle;
    ++success_callbacks_;
  }

  void RemoveServiceSuccessCallback() { ++success_callbacks_; }

  void ErrorCallback(BluetoothServiceRecordBlueZ::ErrorCode code) {
    ++error_callbacks_;
  }

  base::MessageLoop message_loop_;
  std::unique_ptr<base::RunLoop> run_loop_;
  uint32_t last_seen_handle_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothServiceRecordBlueZTest);
};

TEST_F(BluetoothServiceRecordBlueZTest, CreateAndRemove) {
  uint32_t handle1 =
      CreateServiceRecordWithCallbacks(CreateaServiceRecord(kServiceUuid1));
  uint32_t handle2 =
      CreateServiceRecordWithCallbacks(CreateaServiceRecord(kServiceUuid2));
  uint32_t handle3 =
      CreateServiceRecordWithCallbacks(CreateaServiceRecord(kServiceUuid1));
  uint32_t handle4 =
      CreateServiceRecordWithCallbacks(CreateaServiceRecord(kServiceUuid3));

  RemoveServiceRecordWithCallbacks(handle1, true);
  RemoveServiceRecordWithCallbacks(handle3, true);
  RemoveServiceRecordWithCallbacks(handle1, false);
  RemoveServiceRecordWithCallbacks(handle4, true);
  RemoveServiceRecordWithCallbacks(handle2, true);
}

}  // namespace bluez

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "device/core/device_client.h"
#include "device/devices_app/usb/device_impl.h"
#include "device/devices_app/usb/device_manager_impl.h"
#include "device/devices_app/usb/public/cpp/device_manager_delegate.h"
#include "device/usb/mock_usb_device.h"
#include "device/usb/mock_usb_device_handle.h"
#include "device/usb/mock_usb_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_request.h"

using ::testing::Invoke;
using ::testing::_;

namespace device {
namespace usb {

namespace {

class TestDeviceManagerDelegate : public DeviceManagerDelegate {
 public:
  TestDeviceManagerDelegate() {}
  ~TestDeviceManagerDelegate() override {}

 private:
  // DeviceManagerDelegate implementation:
  bool IsDeviceAllowed(const DeviceInfo& device_info) override { return true; }
};

class TestDeviceClient : public DeviceClient {
 public:
  TestDeviceClient() {}
  ~TestDeviceClient() override {}

  MockUsbService& mock_usb_service() { return mock_usb_service_; }

 private:
  // DeviceClient implementation:
  UsbService* GetUsbService() override { return &mock_usb_service_; }

  MockUsbService mock_usb_service_;
};

class USBDeviceManagerImplTest : public testing::Test {
 public:
  USBDeviceManagerImplTest()
      : message_loop_(new base::MessageLoop),
        device_client_(new TestDeviceClient) {}
  ~USBDeviceManagerImplTest() override {}

 protected:
  MockUsbService& mock_usb_service() {
    return device_client_->mock_usb_service();
  }

  DeviceManagerPtr ConnectToDeviceManager() {
    DeviceManagerPtr device_manager;
    new DeviceManagerImpl(
        mojo::GetProxy(&device_manager),
        scoped_ptr<DeviceManagerDelegate>(new TestDeviceManagerDelegate),
        base::ThreadTaskRunnerHandle::Get());
    return device_manager.Pass();
  }

 private:
  scoped_ptr<base::MessageLoop> message_loop_;
  scoped_ptr<TestDeviceClient> device_client_;
};

class MockOpenCallback {
 public:
  explicit MockOpenCallback(UsbDevice* device) : device_(device) {}

  void Open(const UsbDevice::OpenCallback& callback) {
    device_handle_ = new MockUsbDeviceHandle(device_);
    callback.Run(device_handle_);
  }

  scoped_refptr<MockUsbDeviceHandle> mock_handle() { return device_handle_; }

 private:
  UsbDevice* device_;
  scoped_refptr<MockUsbDeviceHandle> device_handle_;
};

void ExpectDevicesAndThen(const std::set<std::string>& expected_guids,
                          const base::Closure& continuation,
                          mojo::Array<DeviceInfoPtr> results) {
  EXPECT_EQ(expected_guids.size(), results.size());
  std::set<std::string> actual_guids;
  for (size_t i = 0; i < results.size(); ++i)
    actual_guids.insert(results[i]->guid);
  EXPECT_EQ(expected_guids, actual_guids);
  continuation.Run();
}

void ExpectDeviceInfoAndThen(const std::string& expected_guid,
                             const base::Closure& continuation,
                             DeviceInfoPtr device_info) {
  EXPECT_EQ(expected_guid, device_info->guid);
  continuation.Run();
}

void ExpectOpenDeviceError(OpenDeviceError expected_error,
                           OpenDeviceError actual_error) {
  EXPECT_EQ(expected_error, actual_error);
}

void FailOnGetDeviceInfoResponse(DeviceInfoPtr device_info) {
  FAIL();
}

}  // namespace

// Test basic GetDevices functionality to ensure that all mock devices are
// returned by the service.
TEST_F(USBDeviceManagerImplTest, GetDevices) {
  scoped_refptr<MockUsbDevice> device0 =
      new MockUsbDevice(0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF");
  scoped_refptr<MockUsbDevice> device1 =
      new MockUsbDevice(0x1234, 0x5679, "ACME", "Frobinator+", "GHIJKL");
  scoped_refptr<MockUsbDevice> device2 =
      new MockUsbDevice(0x1234, 0x567a, "ACME", "Frobinator Mk II", "MNOPQR");

  mock_usb_service().AddDevice(device0);
  mock_usb_service().AddDevice(device1);
  mock_usb_service().AddDevice(device2);

  DeviceManagerPtr device_manager = ConnectToDeviceManager();

  EnumerationOptionsPtr options = EnumerationOptions::New();
  options->filters = mojo::Array<DeviceFilterPtr>::New(1);
  options->filters[0] = DeviceFilter::New();
  options->filters[0]->has_vendor_id = true;
  options->filters[0]->vendor_id = 0x1234;

  std::set<std::string> guids;
  guids.insert(device0->guid());
  guids.insert(device1->guid());
  guids.insert(device2->guid());

  // One call to GetActiveConfiguration for each device during enumeration.
  EXPECT_CALL(*device0.get(), GetActiveConfiguration());
  EXPECT_CALL(*device1.get(), GetActiveConfiguration());
  EXPECT_CALL(*device2.get(), GetActiveConfiguration());

  base::RunLoop loop;
  device_manager->GetDevices(
      options.Pass(),
      base::Bind(&ExpectDevicesAndThen, guids, loop.QuitClosure()));
  loop.Run();
}

// Test requesting a single Device by GUID.
TEST_F(USBDeviceManagerImplTest, OpenDevice) {
  scoped_refptr<MockUsbDevice> mock_device =
      new MockUsbDevice(0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF");

  mock_usb_service().AddDevice(mock_device);

  DeviceManagerPtr device_manager = ConnectToDeviceManager();

  // Should be called on the mock as a result of OpenDevice() below.
  EXPECT_CALL(*mock_device.get(), Open(_));

  MockOpenCallback open_callback(mock_device.get());
  ON_CALL(*mock_device.get(), Open(_))
      .WillByDefault(Invoke(&open_callback, &MockOpenCallback::Open));

  {
    base::RunLoop loop;
    DevicePtr device;
    device_manager->OpenDevice(
        mock_device->guid(), mojo::GetProxy(&device),
        base::Bind(&ExpectOpenDeviceError, OPEN_DEVICE_ERROR_OK));
    device->GetDeviceInfo(base::Bind(&ExpectDeviceInfoAndThen,
                                     mock_device->guid(), loop.QuitClosure()));
    loop.Run();
  }

  // The device should eventually be closed when its MessagePipe is closed.
  DCHECK(open_callback.mock_handle());
  EXPECT_CALL(*open_callback.mock_handle().get(), Close());

  DevicePtr bad_device;
  device_manager->OpenDevice(
      "not a real guid", mojo::GetProxy(&bad_device),
      base::Bind(&ExpectOpenDeviceError, OPEN_DEVICE_ERROR_NOT_FOUND));

  {
    base::RunLoop loop;
    bad_device.set_connection_error_handler(loop.QuitClosure());
    bad_device->GetDeviceInfo(base::Bind(&FailOnGetDeviceInfoResponse));
    loop.Run();
  }
}

}  // namespace usb
}  // namespace device

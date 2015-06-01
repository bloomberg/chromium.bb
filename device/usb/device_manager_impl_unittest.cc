// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "device/core/device_client.h"
#include "device/usb/device_impl.h"
#include "device/usb/device_manager_impl.h"
#include "device/usb/mock_usb_device.h"
#include "device/usb/mock_usb_service.h"
#include "device/usb/public/cpp/device_manager_delegate.h"
#include "device/usb/public/cpp/device_manager_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_request.h"

namespace device {
namespace usb {

namespace {

bool DefaultDelegateFilter(const DeviceInfo& device_info) {
  return true;
}

class TestDeviceManagerDelegate : public DeviceManagerDelegate {
 public:
  using Filter = base::Callback<bool(const DeviceInfo&)>;

  TestDeviceManagerDelegate(const Filter& filter) : filter_(filter) {}
  ~TestDeviceManagerDelegate() override {}

  void set_filter(const Filter& filter) { filter_ = filter; }

 private:
  // DeviceManagerDelegate implementation:
  bool IsDeviceAllowed(const DeviceInfo& device_info) override {
    return filter_.Run(device_info);
  }

  Filter filter_;
};

class TestDeviceClient : public DeviceClient {
 public:
  TestDeviceClient() : delegate_filter_(base::Bind(&DefaultDelegateFilter)) {}
  ~TestDeviceClient() override {}

  MockUsbService& mock_usb_service() { return mock_usb_service_; }

  void SetDelegateFilter(const TestDeviceManagerDelegate::Filter& filter) {
    delegate_filter_ = filter;
  }

 private:
  // DeviceClient implementation:
  UsbService* GetUsbService() override { return &mock_usb_service_; }

  void ConnectToUSBDeviceManager(
      mojo::InterfaceRequest<DeviceManager> request) override {
    new DeviceManagerImpl(request.Pass(),
                          scoped_ptr<DeviceManagerDelegate>(
                              new TestDeviceManagerDelegate(delegate_filter_)));
  }

  TestDeviceManagerDelegate::Filter delegate_filter_;
  MockUsbService mock_usb_service_;
};

class DeviceManagerImplTest : public testing::Test {
 public:
  DeviceManagerImplTest()
      : message_loop_(new base::MessageLoop),
        device_client_(new TestDeviceClient) {}
  ~DeviceManagerImplTest() override {}

 protected:
  MockUsbService& mock_usb_service() {
    return device_client_->mock_usb_service();
  }

  void SetDelegateFilter(const TestDeviceManagerDelegate::Filter& filter) {
    device_client_->SetDelegateFilter(filter);
  }

 private:
  scoped_ptr<base::MessageLoop> message_loop_;
  scoped_ptr<TestDeviceClient> device_client_;
};

void VerifyDevicesAndThen(const std::set<std::string>& expected_guids,
                          scoped_ptr<std::set<std::string>> actual_guids,
                          const base::Closure& continuation) {
  EXPECT_EQ(expected_guids, *actual_guids);
  continuation.Run();
}

void OnGetDeviceInfo(std::set<std::string>* actual_guids,
                     const base::Closure& barrier,
                     DevicePtr device,
                     DeviceInfoPtr info) {
  actual_guids->insert(info->guid);
  barrier.Run();
}

void ExpectDevicesAndThen(const std::set<std::string>& guids,
                          const base::Closure& continuation,
                          mojo::Array<EnumerationResultPtr> results) {
  EXPECT_EQ(guids.size(), results.size());
  scoped_ptr<std::set<std::string>> actual_serials(new std::set<std::string>);
  std::set<std::string>* actual_serials_raw = actual_serials.get();
  base::Closure barrier = base::BarrierClosure(
      static_cast<int>(results.size()),
      base::Bind(&VerifyDevicesAndThen, guids, base::Passed(&actual_serials),
                 continuation));
  for (size_t i = 0; i < results.size(); ++i) {
    DevicePtr device = results[i]->device.Pass();
    Device* raw_device = device.get();
    raw_device->GetDeviceInfo(base::Bind(&OnGetDeviceInfo, actual_serials_raw,
                                         barrier, base::Passed(&device)));
  }
}

}  // namespace

// Test basic GetDevices functionality to ensure that all mock devices are
// returned by the service.
TEST_F(DeviceManagerImplTest, GetDevices) {
  scoped_refptr<MockUsbDevice> device0 =
      new MockUsbDevice(0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF");
  scoped_refptr<MockUsbDevice> device1 =
      new MockUsbDevice(0x1234, 0x5679, "ACME", "Frobinator+", "GHIJKL");
  scoped_refptr<MockUsbDevice> device2 =
      new MockUsbDevice(0x1234, 0x567a, "ACME", "Frobinator Mk II", "MNOPQR");

  mock_usb_service().AddDevice(device0);
  mock_usb_service().AddDevice(device1);
  mock_usb_service().AddDevice(device2);

  DeviceManagerPtr device_manager;
  DeviceClient::Get()->ConnectToUSBDeviceManager(
      mojo::GetProxy(&device_manager));

  EnumerationOptionsPtr options = EnumerationOptions::New();
  options->filters = mojo::Array<DeviceFilterPtr>::New(1);
  options->filters[0] = DeviceFilter::New();
  options->filters[0]->has_vendor_id = true;
  options->filters[0]->vendor_id = 0x1234;

  std::set<std::string> guids;
  guids.insert(device0->guid());
  guids.insert(device1->guid());
  guids.insert(device2->guid());

  EXPECT_CALL(*device0.get(), GetConfiguration());
  EXPECT_CALL(*device1.get(), GetConfiguration());
  EXPECT_CALL(*device2.get(), GetConfiguration());

  base::RunLoop run_loop;
  device_manager->GetDevices(
      options.Pass(),
      base::Bind(&ExpectDevicesAndThen, guids, run_loop.QuitClosure()));
  run_loop.Run();
}

}  // namespace usb
}  // namespace device

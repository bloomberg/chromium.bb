// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_io_thread.h"
#include "device/base/features.h"
#include "device/test/test_device_client.h"
#include "device/test/usb_test_gadget.h"
#include "device/usb/usb_device.h"
#include "device/usb/usb_device_handle.h"
#include "device/usb/usb_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

class UsbServiceTest : public ::testing::Test {
 public:
  UsbServiceTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI),
        io_thread_(base::TestIOThread::kAutoStart) {}

  void SetUp() override {
    device_client_.reset(new TestDeviceClient(io_thread_.task_runner()));
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  base::TestIOThread io_thread_;
  std::unique_ptr<TestDeviceClient> device_client_;
};

void OnGetDevices(const base::Closure& quit_closure,
                  const std::vector<scoped_refptr<UsbDevice>>& devices) {
  quit_closure.Run();
}

void OnOpen(scoped_refptr<UsbDeviceHandle>* output,
            const base::Closure& quit_closure,
            scoped_refptr<UsbDeviceHandle> input) {
  *output = input;
  quit_closure.Run();
}

TEST_F(UsbServiceTest, GetDevices) {
  // Since there's no guarantee that any devices are connected at the moment
  // this test doesn't assume anything about the result but it at least verifies
  // that devices can be enumerated without the application crashing.
  UsbService* service = device_client_->GetUsbService();
  if (service) {
    base::RunLoop loop;
    service->GetDevices(base::Bind(&OnGetDevices, loop.QuitClosure()));
    loop.Run();
  }
}

#if defined(OS_WIN)
TEST_F(UsbServiceTest, GetDevicesNewBackend) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(device::kNewUsbBackend);
  UsbService* service = device_client_->GetUsbService();
  if (service) {
    base::RunLoop loop;
    service->GetDevices(base::Bind(&OnGetDevices, loop.QuitClosure()));
    loop.Run();
  }
}
#endif  // defined(OS_WIN)

TEST_F(UsbServiceTest, ClaimGadget) {
  if (!UsbTestGadget::IsTestEnabled()) return;

  std::unique_ptr<UsbTestGadget> gadget =
      UsbTestGadget::Claim(io_thread_.task_runner());
  ASSERT_TRUE(gadget);

  scoped_refptr<UsbDevice> device = gadget->GetDevice();
  ASSERT_EQ("Google Inc.", base::UTF16ToUTF8(device->manufacturer_string()));
  ASSERT_EQ("Test Gadget (default state)",
            base::UTF16ToUTF8(device->product_string()));
}

TEST_F(UsbServiceTest, DisconnectAndReconnect) {
  if (!UsbTestGadget::IsTestEnabled()) return;

  std::unique_ptr<UsbTestGadget> gadget =
      UsbTestGadget::Claim(io_thread_.task_runner());
  ASSERT_TRUE(gadget);
  ASSERT_TRUE(gadget->Disconnect());
  ASSERT_TRUE(gadget->Reconnect());
}

TEST_F(UsbServiceTest, Shutdown) {
  if (!UsbTestGadget::IsTestEnabled())
    return;

  std::unique_ptr<UsbTestGadget> gadget =
      UsbTestGadget::Claim(io_thread_.task_runner());
  ASSERT_TRUE(gadget);

  base::RunLoop loop;
  scoped_refptr<UsbDeviceHandle> device_handle;
  gadget->GetDevice()->Open(
      base::Bind(&OnOpen, &device_handle, loop.QuitClosure()));
  loop.Run();
  ASSERT_TRUE(device_handle);

  // Shut down the USB service while the device handle is still open.
  device_client_.reset();
  EXPECT_FALSE(device_handle->GetDevice());
}

}  // namespace

}  // namespace device

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "components/usb_service/usb_device.h"
#include "components/usb_service/usb_device_handle.h"
#include "device/test/usb_test_gadget.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::usb_service::UsbDeviceHandle;

namespace device {

namespace {

class UsbServiceTest : public ::testing::Test {
 public:
  virtual void SetUp() {
    message_loop_.reset(new base::MessageLoopForIO);
  }

 private:
  scoped_ptr<base::MessageLoop> message_loop_;
};

TEST_F(UsbServiceTest, ClaimGadget) {
  if (!UsbTestGadget::IsTestEnabled()) return;

  scoped_ptr<UsbTestGadget> gadget = UsbTestGadget::Claim();
  ASSERT_TRUE(gadget.get());

  scoped_refptr<UsbDeviceHandle> handle = gadget->GetDevice()->Open();

  base::string16 utf16;
  ASSERT_TRUE(handle->GetManufacturer(&utf16));
  ASSERT_EQ("Google Inc.", base::UTF16ToUTF8(utf16));
  // Check again to make sure string descriptor caching works.
  ASSERT_EQ("Google Inc.", base::UTF16ToUTF8(utf16));

  ASSERT_TRUE(handle->GetProduct(&utf16));
  ASSERT_EQ("Test Gadget (default state)", base::UTF16ToUTF8(utf16));
  // Check again to make sure string descriptor caching works.
  ASSERT_EQ("Test Gadget (default state)", base::UTF16ToUTF8(utf16));

  ASSERT_TRUE(handle->GetSerial(&utf16));
  ASSERT_EQ(gadget->GetSerial(), base::UTF16ToUTF8(utf16));
  // Check again to make sure string descriptor caching works.
  ASSERT_EQ(gadget->GetSerial(), base::UTF16ToUTF8(utf16));
}

TEST_F(UsbServiceTest, DisconnectAndReconnect) {
  if (!UsbTestGadget::IsTestEnabled()) return;

  scoped_ptr<UsbTestGadget> gadget = UsbTestGadget::Claim();
  ASSERT_TRUE(gadget.get());
  ASSERT_TRUE(gadget->Disconnect());
  ASSERT_TRUE(gadget->Reconnect());
}

}  // namespace

}  // namespace device

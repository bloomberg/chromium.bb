// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "device/test/usb_test_gadget.h"
#include "device/usb/usb_device.h"
#include "device/usb/usb_device_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

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

  scoped_refptr<UsbDevice> device = gadget->GetDevice();
  base::string16 utf16;
  ASSERT_TRUE(device->GetManufacturer(&utf16));
  ASSERT_EQ("Google Inc.", base::UTF16ToUTF8(utf16));

  ASSERT_TRUE(device->GetProduct(&utf16));
  ASSERT_EQ("Test Gadget (default state)", base::UTF16ToUTF8(utf16));

  ASSERT_TRUE(device->GetSerialNumber(&utf16));
  ASSERT_EQ(gadget->GetSerialNumber(), base::UTF16ToUTF8(utf16));
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

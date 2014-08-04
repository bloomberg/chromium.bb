// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "device/hid/hid_connection.h"
#include "device/hid/hid_service.h"
#include "device/test/usb_test_gadget.h"
#include "net/base/io_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

using net::IOBufferWithSize;

class TestCompletionCallback {
 public:
  TestCompletionCallback()
    : callback_(base::Bind(&TestCompletionCallback::SetResult,
                base::Unretained(this))) {}

  void SetResult(bool success, size_t size) {
    result_ = success;
    transferred_ = size;
    run_loop_.Quit();
  }

  bool WaitForResult() {
    run_loop_.Run();
    return result_;
  }

  const HidConnection::IOCallback& callback() const { return callback_; }
  size_t transferred() const { return transferred_; }

 private:
  const HidConnection::IOCallback callback_;
  base::RunLoop run_loop_;
  bool result_;
  size_t transferred_;
};

}  // namespace

class HidConnectionTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    if (!UsbTestGadget::IsTestEnabled()) return;

    message_loop_.reset(new base::MessageLoopForIO());
    service_.reset(HidService::Create(message_loop_->message_loop_proxy()));
    ASSERT_TRUE(service_);

    test_gadget_ = UsbTestGadget::Claim();
    ASSERT_TRUE(test_gadget_);
    ASSERT_TRUE(test_gadget_->SetType(UsbTestGadget::HID_ECHO));

    device_id_ = kInvalidHidDeviceId;

    base::RunLoop run_loop;
    message_loop_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&HidConnectionTest::FindDevice,
                   base::Unretained(this), run_loop.QuitClosure(), 5),
        base::TimeDelta::FromMilliseconds(250));
    run_loop.Run();

    ASSERT_NE(device_id_, kInvalidHidDeviceId);
  }

  void FindDevice(const base::Closure& done, int retries) {
    std::vector<HidDeviceInfo> devices;
    service_->GetDevices(&devices);

    for (std::vector<HidDeviceInfo>::iterator it = devices.begin();
         it != devices.end();
         ++it) {
      if (it->serial_number == test_gadget_->GetSerial()) {
        device_id_ = it->device_id;
        break;
      }
    }

    if (device_id_ == kInvalidHidDeviceId && --retries > 0) {
      message_loop_->PostDelayedTask(
          FROM_HERE,
          base::Bind(&HidConnectionTest::FindDevice, base::Unretained(this),
                     done, retries),
          base::TimeDelta::FromMilliseconds(10));
    } else {
      message_loop_->PostTask(FROM_HERE, done);
    }
  }

  scoped_ptr<base::MessageLoopForIO> message_loop_;
  scoped_ptr<HidService> service_;
  scoped_ptr<UsbTestGadget> test_gadget_;
  HidDeviceId device_id_;
};

TEST_F(HidConnectionTest, ReadWrite) {
  if (!UsbTestGadget::IsTestEnabled()) return;

  scoped_refptr<HidConnection> conn = service_->Connect(device_id_);
  ASSERT_TRUE(conn);

  for (int i = 0; i < 8; ++i) {
    scoped_refptr<IOBufferWithSize> write_buffer(new IOBufferWithSize(8));
    *(int64_t*)write_buffer->data() = i;

    TestCompletionCallback write_callback;
    conn->Write(0, write_buffer, write_callback.callback());
    ASSERT_TRUE(write_callback.WaitForResult());
    ASSERT_EQ(8UL, write_callback.transferred());

    scoped_refptr<IOBufferWithSize> read_buffer(new IOBufferWithSize(8));
    TestCompletionCallback read_callback;
    conn->Read(read_buffer, read_callback.callback());
    ASSERT_TRUE(read_callback.WaitForResult());
    ASSERT_EQ(8UL, read_callback.transferred());
    ASSERT_EQ(i, *(int64_t*)read_buffer->data());
  }
}

}  // namespace device

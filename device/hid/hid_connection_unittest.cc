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
      : read_callback_(base::Bind(&TestCompletionCallback::SetReadResult,
                                  base::Unretained(this))),
        write_callback_(base::Bind(&TestCompletionCallback::SetWriteResult,
                                   base::Unretained(this))) {}
  ~TestCompletionCallback() {}

  void SetReadResult(bool success,
                     scoped_refptr<net::IOBuffer> buffer,
                     size_t size) {
    result_ = success;
    buffer_ = buffer;
    size_ = size;
    run_loop_.Quit();
  }

  void SetWriteResult(bool success) {
    result_ = success;
    run_loop_.Quit();
  }

  bool WaitForResult() {
    run_loop_.Run();
    return result_;
  }

  const HidConnection::ReadCallback& read_callback() { return read_callback_; }
  const HidConnection::WriteCallback write_callback() {
    return write_callback_;
  }
  scoped_refptr<net::IOBuffer> buffer() const { return buffer_; }
  size_t size() const { return size_; }

 private:
  base::RunLoop run_loop_;
  bool result_;
  size_t size_;
  scoped_refptr<net::IOBuffer> buffer_;
  HidConnection::ReadCallback read_callback_;
  HidConnection::WriteCallback write_callback_;
};

}  // namespace

class HidConnectionTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    if (!UsbTestGadget::IsTestEnabled()) return;

    message_loop_.reset(new base::MessageLoopForIO());
    service_ = HidService::GetInstance(message_loop_->message_loop_proxy());
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
      if (it->serial_number == test_gadget_->GetSerialNumber()) {
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
  HidService* service_;
  scoped_ptr<UsbTestGadget> test_gadget_;
  HidDeviceId device_id_;
};

TEST_F(HidConnectionTest, ReadWrite) {
  if (!UsbTestGadget::IsTestEnabled()) return;

  scoped_refptr<HidConnection> conn = service_->Connect(device_id_);
  ASSERT_TRUE(conn.get());

  for (int i = 0; i < 8; ++i) {
    scoped_refptr<IOBufferWithSize> buffer(new IOBufferWithSize(9));
    buffer->data()[0] = 0;
    for (int j = 1; j < buffer->size(); ++j) {
      buffer->data()[j] = i + j - 1;
    }

    TestCompletionCallback write_callback;
    conn->Write(buffer, buffer->size(), write_callback.write_callback());
    ASSERT_TRUE(write_callback.WaitForResult());

    TestCompletionCallback read_callback;
    conn->Read(read_callback.read_callback());
    ASSERT_TRUE(read_callback.WaitForResult());
    ASSERT_EQ(9UL, read_callback.size());
    ASSERT_EQ(0, read_callback.buffer()->data()[0]);
    for (int j = 1; j < buffer->size(); ++j) {
      ASSERT_EQ(i + j - 1, read_callback.buffer()->data()[j]);
    }
  }
}

}  // namespace device

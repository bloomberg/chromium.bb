// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "device/test/usb_test_gadget.h"
#include "device/usb/usb_device.h"
#include "device/usb/usb_device_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

class UsbDeviceHandleTest : public ::testing::Test {
 public:
  virtual void SetUp() {
    if (!UsbTestGadget::IsTestEnabled()) {
      return;
    }

    message_loop_.reset(new base::MessageLoopForIO);

    gadget_ = UsbTestGadget::Claim();
    ASSERT_TRUE(gadget_.get());

    ASSERT_TRUE(gadget_->SetType(UsbTestGadget::ECHO));

    handle_ = gadget_->GetDevice()->Open();
    ASSERT_TRUE(handle_.get());
  }

  virtual void TearDown() {
    if (handle_.get()) {
      handle_->Close();
    }
    gadget_.reset(NULL);
    message_loop_.reset(NULL);
  }

 protected:
  scoped_refptr<UsbDeviceHandle> handle_;

 private:
  scoped_ptr<UsbTestGadget> gadget_;
  scoped_ptr<base::MessageLoop> message_loop_;
};

class TestCompletionCallback {
 public:
  TestCompletionCallback()
      : callback_(base::Bind(&TestCompletionCallback::SetResult,
                             base::Unretained(this))) {}

  void SetResult(UsbTransferStatus status,
                 scoped_refptr<net::IOBuffer> buffer,
                 size_t transferred) {
    status_ = status;
    transferred_ = transferred;
    run_loop_.Quit();
  }

  void WaitForResult() { run_loop_.Run(); }

  const UsbTransferCallback& callback() const { return callback_; }
  UsbTransferStatus status() const { return status_; }
  size_t transferred() const { return transferred_; }

 private:
  const UsbTransferCallback callback_;
  base::RunLoop run_loop_;
  UsbTransferStatus status_;
  size_t transferred_;
};

TEST_F(UsbDeviceHandleTest, InterruptTransfer) {
  if (!handle_.get()) {
    return;
  }

  scoped_refptr<net::IOBufferWithSize> in_buffer(new net::IOBufferWithSize(64));
  TestCompletionCallback in_completion;
  handle_->InterruptTransfer(USB_DIRECTION_INBOUND,
                             0x81,
                             in_buffer.get(),
                             in_buffer->size(),
                             5000,  // 5 second timeout
                             in_completion.callback());

  scoped_refptr<net::IOBufferWithSize> out_buffer(
      new net::IOBufferWithSize(in_buffer->size()));
  TestCompletionCallback out_completion;
  for (int i = 0; i < out_buffer->size(); ++i) {
    out_buffer->data()[i] = i;
  }

  handle_->InterruptTransfer(USB_DIRECTION_OUTBOUND,
                             0x01,
                             out_buffer.get(),
                             out_buffer->size(),
                             5000,  // 5 second timeout
                             out_completion.callback());
  out_completion.WaitForResult();
  ASSERT_EQ(USB_TRANSFER_COMPLETED, out_completion.status());
  ASSERT_EQ(static_cast<size_t>(out_buffer->size()),
            out_completion.transferred());

  in_completion.WaitForResult();
  ASSERT_EQ(USB_TRANSFER_COMPLETED, in_completion.status());
  ASSERT_EQ(static_cast<size_t>(in_buffer->size()),
            in_completion.transferred());
  for (int i = 0; i < in_buffer->size(); ++i) {
    ASSERT_EQ(out_buffer->data()[i], in_buffer->data()[i]);
  }
}

TEST_F(UsbDeviceHandleTest, BulkTransfer) {
  if (!handle_.get()) {
    return;
  }

  scoped_refptr<net::IOBufferWithSize> in_buffer(
      new net::IOBufferWithSize(512));
  TestCompletionCallback in_completion;
  handle_->BulkTransfer(USB_DIRECTION_INBOUND,
                        0x81,
                        in_buffer.get(),
                        in_buffer->size(),
                        5000,  // 5 second timeout
                        in_completion.callback());

  scoped_refptr<net::IOBufferWithSize> out_buffer(
      new net::IOBufferWithSize(in_buffer->size()));
  TestCompletionCallback out_completion;
  for (int i = 0; i < out_buffer->size(); ++i) {
    out_buffer->data()[i] = i;
  }

  handle_->BulkTransfer(USB_DIRECTION_OUTBOUND,
                        0x01,
                        out_buffer.get(),
                        out_buffer->size(),
                        5000,  // 5 second timeout
                        out_completion.callback());
  out_completion.WaitForResult();
  ASSERT_EQ(USB_TRANSFER_COMPLETED, out_completion.status());
  ASSERT_EQ(static_cast<size_t>(out_buffer->size()),
            out_completion.transferred());

  in_completion.WaitForResult();
  ASSERT_EQ(USB_TRANSFER_COMPLETED, in_completion.status());
  ASSERT_EQ(static_cast<size_t>(in_buffer->size()),
            in_completion.transferred());
  for (int i = 0; i < in_buffer->size(); ++i) {
    ASSERT_EQ(out_buffer->data()[i], in_buffer->data()[i]);
  }
}

}  // namespace

}  // namespace device

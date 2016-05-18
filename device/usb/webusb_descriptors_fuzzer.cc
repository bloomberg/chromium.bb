// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/i18n/icu_util.h"
#include "device/usb/mock_usb_device.h"
#include "device/usb/mock_usb_device_handle.h"
#include "device/usb/webusb_descriptors.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::Invoke;

struct TestCase {
  TestCase() { CHECK(base::i18n::InitializeICU()); }

  // used by ICU integration.
  base::AtExitManager at_exit_manager;
};

TestCase* test_case = new TestCase();

namespace device {

class Helper {
 public:
  Helper(const unsigned char* data, size_t size) : data_(data), size_(size) {}

  void ControlTransfer(UsbEndpointDirection direction,
                       UsbDeviceHandle::TransferRequestType request_type,
                       UsbDeviceHandle::TransferRecipient recipient,
                       uint8_t request,
                       uint16_t value,
                       uint16_t index,
                       scoped_refptr<net::IOBuffer> buffer,
                       size_t length,
                       unsigned int timeout,
                       const UsbDeviceHandle::TransferCallback& callback) {
    if (position_ == size_) {
      callback.Run(USB_TRANSFER_DISCONNECT, buffer, 0);
      return;
    }

    if (data_[position_++] % 2) {
      size_t bytes_transferred = 0;
      if (position_ + 2 <= size_) {
        bytes_transferred = data_[position_] | data_[position_ + 1] << 8;
        position_ += 2;
        bytes_transferred = std::min(bytes_transferred, length);
        bytes_transferred = std::min(bytes_transferred, size_ - position_);
      }

      if (direction == USB_DIRECTION_INBOUND) {
        memcpy(buffer->data(), &data_[position_], bytes_transferred);
        position_ += bytes_transferred;
      }

      callback.Run(USB_TRANSFER_COMPLETED, buffer, bytes_transferred);
    } else {
      callback.Run(USB_TRANSFER_ERROR, buffer, 0);
    }
  }

 private:
  const unsigned char* const data_;
  const size_t size_;
  size_t position_ = 0;
};

void Done(std::unique_ptr<WebUsbAllowedOrigins> allowed_origins,
          const GURL& landing_page) {}

extern "C" int LLVMFuzzerTestOneInput(const unsigned char* data, size_t size) {
  scoped_refptr<MockUsbDevice> device = new MockUsbDevice(0, 0);
  scoped_refptr<MockUsbDeviceHandle> device_handle =
      new MockUsbDeviceHandle(device.get());
  Helper helper(data, size);
  EXPECT_CALL(*device_handle, ControlTransfer(_, _, _, _, _, _, _, _, _, _))
      .WillRepeatedly(Invoke(&helper, &Helper::ControlTransfer));

  ReadWebUsbDescriptors(device_handle, base::Bind(&Done));
  return 0;
}

}  // namespace device

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/usb/usb_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::AnyNumber;
using testing::_;
using testing::Return;
using content::BrowserThread;

namespace {

ACTION_TEMPLATE(InvokeUsbTransferCallback,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(p1)) {
  ::std::tr1::get<k>(args).Run(p1, new net::IOBuffer(1), 1);
}

// MSVC erroneously thinks that at least one of the arguments for the transfer
// methods differ by const or volatility and emits a warning about the old
// standards-noncompliant behaviour of their compiler.
#if defined(OS_WIN)
#pragma warning(push)
#pragma warning(disable:4373)
#endif

class MockUsbDeviceHandle : public UsbDeviceHandle {
 public:
  MockUsbDeviceHandle() : UsbDeviceHandle() {}

  MOCK_METHOD0(Close, void());

  MOCK_METHOD10(ControlTransfer, void(const UsbEndpointDirection direction,
      const TransferRequestType request_type, const TransferRecipient recipient,
      const uint8 request, const uint16 value, const uint16 index,
      net::IOBuffer* buffer, const size_t length, const unsigned int timeout,
      const UsbTransferCallback& callback));

  MOCK_METHOD6(BulkTransfer, void(const UsbEndpointDirection direction,
      const uint8 endpoint, net::IOBuffer* buffer, const size_t length,
      const unsigned int timeout, const UsbTransferCallback& callback));

  MOCK_METHOD6(InterruptTransfer, void(const UsbEndpointDirection direction,
      const uint8 endpoint, net::IOBuffer* buffer, const size_t length,
      const unsigned int timeout, const UsbTransferCallback& callback));

  MOCK_METHOD8(IsochronousTransfer, void(const UsbEndpointDirection direction,
      const uint8 endpoint, net::IOBuffer* buffer, const size_t length,
      const unsigned int packets, const unsigned int packet_length,
      const unsigned int timeout, const UsbTransferCallback& callback));

  MOCK_METHOD0(ResetDevice, bool());

  void set_device(UsbDevice* device) { device_ = device; }

 protected:
  virtual ~MockUsbDeviceHandle() {}
};

class MockUsbDevice : public UsbDevice {
 public:
  explicit MockUsbDevice(MockUsbDeviceHandle* mock_handle)
     : UsbDevice(),
       mock_handle_(mock_handle) {
    mock_handle->set_device(this);
  }

  virtual scoped_refptr<UsbDeviceHandle> Open() OVERRIDE {
    return mock_handle_;
  }

  virtual bool Close(scoped_refptr<UsbDeviceHandle> handle) OVERRIDE {
    EXPECT_TRUE(false) << "Should not be reached";
    return false;
  }

  MOCK_METHOD0(ListInterfaces, scoped_refptr<UsbConfigDescriptor>());

 private:
  MockUsbDeviceHandle* mock_handle_;
  virtual ~MockUsbDevice() {}
};

#if defined(OS_WIN)
#pragma warning(pop)
#endif

class UsbApiTest : public ExtensionApiTest {
 public:
  virtual void SetUpOnMainThread() OVERRIDE {
    mock_device_handle_ = new MockUsbDeviceHandle();
    mock_device_ = new MockUsbDevice(mock_device_handle_.get());
    extensions::UsbFindDevicesFunction::SetDeviceForTest(mock_device_.get());
  }

 protected:
  scoped_refptr<MockUsbDeviceHandle> mock_device_handle_;
  scoped_refptr<MockUsbDevice> mock_device_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(UsbApiTest, DeviceHandling) {
  EXPECT_CALL(*mock_device_handle_.get(), Close()).Times(AnyNumber());
  EXPECT_CALL(*mock_device_handle_.get(), ResetDevice())
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  EXPECT_CALL(*mock_device_handle_.get(),
              InterruptTransfer(USB_DIRECTION_OUTBOUND, 2, _, 1, _, _))
      .WillOnce(InvokeUsbTransferCallback<5>(USB_TRANSFER_COMPLETED));
  ASSERT_TRUE(RunExtensionTest("usb/device_handling"));
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, ListInterfaces) {
  EXPECT_CALL(*mock_device_.get(), ListInterfaces())
      .WillOnce(Return(scoped_refptr<UsbConfigDescriptor>()));
  EXPECT_CALL(*mock_device_handle_.get(), Close()).Times(AnyNumber());
  ASSERT_TRUE(RunExtensionTest("usb/list_interfaces"));
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, TransferEvent) {
  EXPECT_CALL(*mock_device_handle_.get(),
              ControlTransfer(USB_DIRECTION_OUTBOUND,
                              UsbDeviceHandle::STANDARD,
                              UsbDeviceHandle::DEVICE,
                              1,
                              2,
                              3,
                              _,
                              1,
                              _,
                              _))
      .WillOnce(InvokeUsbTransferCallback<9>(USB_TRANSFER_COMPLETED));
  EXPECT_CALL(*mock_device_handle_.get(),
              BulkTransfer(USB_DIRECTION_OUTBOUND, 1, _, 1, _, _))
      .WillOnce(InvokeUsbTransferCallback<5>(USB_TRANSFER_COMPLETED));
  EXPECT_CALL(*mock_device_handle_.get(),
              InterruptTransfer(USB_DIRECTION_OUTBOUND, 2, _, 1, _, _))
      .WillOnce(InvokeUsbTransferCallback<5>(USB_TRANSFER_COMPLETED));
  EXPECT_CALL(*mock_device_handle_.get(),
              IsochronousTransfer(USB_DIRECTION_OUTBOUND, 3, _, 1, 1, 1, _, _))
      .WillOnce(InvokeUsbTransferCallback<7>(USB_TRANSFER_COMPLETED));
  EXPECT_CALL(*mock_device_handle_.get(), Close()).Times(AnyNumber());
  ASSERT_TRUE(RunExtensionTest("usb/transfer_event"));
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, ZeroLengthTransfer) {
  EXPECT_CALL(*mock_device_handle_.get(), BulkTransfer(_, _, _, 0, _, _))
      .WillOnce(InvokeUsbTransferCallback<5>(USB_TRANSFER_COMPLETED));
  EXPECT_CALL(*mock_device_handle_.get(), Close()).Times(AnyNumber());
  ASSERT_TRUE(RunExtensionTest("usb/zero_length_transfer"));
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, TransferFailure) {
  EXPECT_CALL(*mock_device_handle_.get(), BulkTransfer(_, _, _, _, _, _))
      .WillOnce(InvokeUsbTransferCallback<5>(USB_TRANSFER_COMPLETED))
      .WillOnce(InvokeUsbTransferCallback<5>(USB_TRANSFER_ERROR))
      .WillOnce(InvokeUsbTransferCallback<5>(USB_TRANSFER_TIMEOUT));
  EXPECT_CALL(*mock_device_handle_.get(), Close()).Times(AnyNumber());
  ASSERT_TRUE(RunExtensionTest("usb/transfer_failure"));
}

IN_PROC_BROWSER_TEST_F(UsbApiTest, InvalidLengthTransfer) {
  EXPECT_CALL(*mock_device_handle_.get(), Close()).Times(AnyNumber());
  ASSERT_TRUE(RunExtensionTest("usb/invalid_length_transfer"));
}

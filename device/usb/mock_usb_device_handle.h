// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_MOCK_USB_DEVICE_HANDLE_H_
#define DEVICE_USB_MOCK_USB_DEVICE_HANDLE_H_

#include "device/usb/usb_device_handle.h"

#include <stddef.h>
#include <stdint.h>

#include "net/base/io_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {

class MockUsbDeviceHandle : public UsbDeviceHandle {
 public:
  MockUsbDeviceHandle(UsbDevice* device);

  scoped_refptr<UsbDevice> GetDevice() const override;
  MOCK_METHOD0(Close, void());
  MOCK_METHOD2(SetConfiguration,
               void(int configuration_value, const ResultCallback& callback));
  MOCK_METHOD2(ClaimInterface,
               void(int interface_number, const ResultCallback& callback));
  MOCK_METHOD1(ReleaseInterface, bool(int interface_number));
  MOCK_METHOD3(SetInterfaceAlternateSetting,
               void(int interface_number,
                    int alternate_setting,
                    const ResultCallback& callback));
  MOCK_METHOD1(ResetDevice, void(const ResultCallback& callback));
  MOCK_METHOD2(ClearHalt,
               void(uint8_t endpoint, const ResultCallback& callback));
  MOCK_METHOD10(ControlTransfer,
                void(UsbEndpointDirection direction,
                     TransferRequestType request_type,
                     TransferRecipient recipient,
                     uint8_t request,
                     uint16_t value,
                     uint16_t index,
                     scoped_refptr<net::IOBuffer> buffer,
                     size_t length,
                     unsigned int timeout,
                     const TransferCallback& callback));
  MOCK_METHOD8(IsochronousTransfer,
               void(UsbEndpointDirection direction,
                    uint8_t endpoint,
                    scoped_refptr<net::IOBuffer> buffer,
                    size_t length,
                    unsigned int packets,
                    unsigned int packet_length,
                    unsigned int timeout,
                    const TransferCallback& callback));
  MOCK_METHOD6(GenericTransfer,
               void(UsbEndpointDirection direction,
                    uint8_t endpoint,
                    scoped_refptr<net::IOBuffer> buffer,
                    size_t length,
                    unsigned int timeout,
                    const TransferCallback& callback));
  MOCK_METHOD2(FindInterfaceByEndpoint,
               bool(uint8_t endpoint_address, uint8_t* interface_number));

 private:
  ~MockUsbDeviceHandle() override;

  UsbDevice* device_;
};

}  // namespace device

#endif  // DEVICE_USB_MOCK_USB_DEVICE_HANDLE_H_

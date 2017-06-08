// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_FAKE_USB_DEVICE_HANDLE_H_
#define DEVICE_USB_FAKE_USB_DEVICE_HANDLE_H_

#include "device/usb/usb_device_handle.h"

namespace device {

// This class implements a fake USB device handle that will handle control
// requests by responding with data out of the provided buffer. The format of
// each record in the buffer is:
//
// 0         1         2         3...
// +---------+---------+---------+---------- - - -
// | success |      length       | data...
// +---------+---------+---------+---------- - - -
//
// If |success| is 1 the transfer will succeed and respond with |length| bytes
// of the following data (no data is consumed for outgoing transfers). If
// |success| is 0 the transfer will fail with USB_TRANSFER_ERROR. If the buffer
// is exhausted all following transfers will fail with USB_TRANSFER_DISCONNECT.
class FakeUsbDeviceHandle : public UsbDeviceHandle {
 public:
  FakeUsbDeviceHandle(const uint8_t* data, size_t size);

  scoped_refptr<UsbDevice> GetDevice() const override;
  void Close() override;
  void SetConfiguration(int configuration_value,
                        const ResultCallback& callback) override;
  void ClaimInterface(int interface_number,
                      const ResultCallback& callback) override;
  void ReleaseInterface(int interface_number,
                        const ResultCallback& callback) override;
  void SetInterfaceAlternateSetting(int interface_number,
                                    int alternate_setting,
                                    const ResultCallback& callback) override;
  void ResetDevice(const ResultCallback& callback) override;
  void ClearHalt(uint8_t endpoint, const ResultCallback& callback) override;

  void ControlTransfer(UsbTransferDirection direction,
                       UsbControlTransferType request_type,
                       UsbControlTransferRecipient recipient,
                       uint8_t request,
                       uint16_t value,
                       uint16_t index,
                       scoped_refptr<net::IOBuffer> buffer,
                       size_t length,
                       unsigned int timeout,
                       const TransferCallback& callback) override;

  void IsochronousTransferIn(
      uint8_t endpoint,
      const std::vector<uint32_t>& packet_lengths,
      unsigned int timeout,
      const IsochronousTransferCallback& callback) override;

  void IsochronousTransferOut(
      uint8_t endpoint,
      scoped_refptr<net::IOBuffer> buffer,
      const std::vector<uint32_t>& packet_lengths,
      unsigned int timeout,
      const IsochronousTransferCallback& callback) override;

  void GenericTransfer(UsbTransferDirection direction,
                       uint8_t endpoint_number,
                       scoped_refptr<net::IOBuffer> buffer,
                       size_t length,
                       unsigned int timeout,
                       const TransferCallback& callback) override;
  const UsbInterfaceDescriptor* FindInterfaceByEndpoint(
      uint8_t endpoint_address) override;

 private:
  ~FakeUsbDeviceHandle() override;

  const uint8_t* const data_;
  const size_t size_;
  size_t position_;
};

}  // namespace device

#endif  // DEVICE_USB_FAKE_USB_DEVICE_HANDLE_H_

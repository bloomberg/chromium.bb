// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_USB_DEVICE_HANDLE_WIN_H_
#define DEVICE_USB_USB_DEVICE_HANDLE_WIN_H_

#include <unordered_map>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/win/scoped_handle.h"
#include "device/usb/usb_device_handle.h"

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}

namespace net {
class IOBuffer;
}

namespace device {

class UsbDeviceWin;

// UsbDeviceHandle class provides basic I/O related functionalities.
class UsbDeviceHandleWin : public UsbDeviceHandle {
 public:
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

  void ControlTransfer(UsbEndpointDirection direction,
                       TransferRequestType request_type,
                       TransferRecipient recipient,
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

  void GenericTransfer(UsbEndpointDirection direction,
                       uint8_t endpoint_number,
                       scoped_refptr<net::IOBuffer> buffer,
                       size_t length,
                       unsigned int timeout,
                       const TransferCallback& callback) override;
  const UsbInterfaceDescriptor* FindInterfaceByEndpoint(
      uint8_t endpoint_address) override;

 protected:
  friend class UsbDeviceWin;

  // Constructor used to build a connection to the device's parent hub.
  UsbDeviceHandleWin(
      scoped_refptr<UsbDeviceWin> device,
      base::win::ScopedHandle handle,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner);

  ~UsbDeviceHandleWin() override;

 private:
  class Request;

  Request* MakeRequest(HANDLE handle);
  std::unique_ptr<Request> UnlinkRequest(Request* request);
  void GotNodeConnectionInformation(const TransferCallback& callback,
                                    void* node_connection_info,
                                    scoped_refptr<net::IOBuffer> buffer,
                                    size_t buffer_length,
                                    Request* request_ptr,
                                    DWORD win32_result,
                                    size_t bytes_transferred);
  void GotDescriptorFromNodeConnection(
      const TransferCallback& callback,
      scoped_refptr<net::IOBuffer> request_buffer,
      scoped_refptr<net::IOBuffer> original_buffer,
      size_t original_buffer_length,
      Request* request_ptr,
      DWORD win32_result,
      size_t bytes_transferred);

  base::ThreadChecker thread_checker_;

  scoped_refptr<UsbDeviceWin> device_;

  // |hub_handle_| must outlive |requests_| because individual Request objects
  // hold on to the handle for the purpose of calling GetOverlappedResult().
  base::win::ScopedHandle hub_handle_;
  std::unordered_map<Request*, std::unique_ptr<Request>> requests_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  base::WeakPtrFactory<UsbDeviceHandleWin> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UsbDeviceHandleWin);
};

}  // namespace device

#endif  // DEVICE_USB_USB_DEVICE_HANDLE_WIN_H_

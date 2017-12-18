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
class RefCountedBytes;
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}

namespace device {

class UsbDeviceWin;

// UsbDeviceHandle class provides basic I/O related functionalities.
class UsbDeviceHandleWin : public UsbDeviceHandle {
 public:
  scoped_refptr<UsbDevice> GetDevice() const override;
  void Close() override;
  void SetConfiguration(int configuration_value,
                        ResultCallback callback) override;
  void ClaimInterface(int interface_number, ResultCallback callback) override;
  void ReleaseInterface(int interface_number, ResultCallback callback) override;
  void SetInterfaceAlternateSetting(int interface_number,
                                    int alternate_setting,
                                    ResultCallback callback) override;
  void ResetDevice(ResultCallback callback) override;
  void ClearHalt(uint8_t endpoint, ResultCallback callback) override;

  void ControlTransfer(UsbTransferDirection direction,
                       UsbControlTransferType request_type,
                       UsbControlTransferRecipient recipient,
                       uint8_t request,
                       uint16_t value,
                       uint16_t index,
                       scoped_refptr<base::RefCountedBytes> buffer,
                       unsigned int timeout,
                       TransferCallback callback) override;

  void IsochronousTransferIn(uint8_t endpoint,
                             const std::vector<uint32_t>& packet_lengths,
                             unsigned int timeout,
                             IsochronousTransferCallback callback) override;

  void IsochronousTransferOut(uint8_t endpoint,
                              scoped_refptr<base::RefCountedBytes> buffer,
                              const std::vector<uint32_t>& packet_lengths,
                              unsigned int timeout,
                              IsochronousTransferCallback callback) override;

  void GenericTransfer(UsbTransferDirection direction,
                       uint8_t endpoint_number,
                       scoped_refptr<base::RefCountedBytes> buffer,
                       unsigned int timeout,
                       TransferCallback callback) override;
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
  void GotNodeConnectionInformation(TransferCallback callback,
                                    void* node_connection_info,
                                    scoped_refptr<base::RefCountedBytes> buffer,
                                    Request* request_ptr,
                                    DWORD win32_result,
                                    size_t bytes_transferred);
  void GotDescriptorFromNodeConnection(
      TransferCallback callback,
      scoped_refptr<base::RefCountedBytes> request_buffer,
      scoped_refptr<base::RefCountedBytes> original_buffer,
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

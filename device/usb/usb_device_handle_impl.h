// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_USB_DEVICE_HANDLE_IMPL_H_
#define DEVICE_USB_USB_DEVICE_HANDLE_IMPL_H_

#include <map>
#include <set>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/threading/thread_checker.h"
#include "device/usb/usb_device_handle.h"
#include "net/base/io_buffer.h"
#include "third_party/libusb/src/libusb/libusb.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace device {

class UsbContext;
struct UsbConfigDescriptor;
class UsbDeviceImpl;

typedef libusb_device_handle* PlatformUsbDeviceHandle;
typedef libusb_iso_packet_descriptor* PlatformUsbIsoPacketDescriptor;
typedef libusb_transfer* PlatformUsbTransferHandle;

// UsbDeviceHandle class provides basic I/O related functionalities.
class UsbDeviceHandleImpl : public UsbDeviceHandle {
 public:
  scoped_refptr<UsbDevice> GetDevice() const override;
  void Close() override;
  bool SetConfiguration(int configuration_value) override;
  bool ClaimInterface(int interface_number) override;
  bool ReleaseInterface(int interface_number) override;
  bool SetInterfaceAlternateSetting(int interface_number,
                                    int alternate_setting) override;
  bool ResetDevice() override;
  bool GetStringDescriptor(uint8 string_id, base::string16* string) override;

  void ControlTransfer(UsbEndpointDirection direction,
                       TransferRequestType request_type,
                       TransferRecipient recipient,
                       uint8 request,
                       uint16 value,
                       uint16 index,
                       net::IOBuffer* buffer,
                       size_t length,
                       unsigned int timeout,
                       const UsbTransferCallback& callback) override;

  void BulkTransfer(UsbEndpointDirection direction,
                    uint8 endpoint,
                    net::IOBuffer* buffer,
                    size_t length,
                    unsigned int timeout,
                    const UsbTransferCallback& callback) override;

  void InterruptTransfer(UsbEndpointDirection direction,
                         uint8 endpoint,
                         net::IOBuffer* buffer,
                         size_t length,
                         unsigned int timeout,
                         const UsbTransferCallback& callback) override;

  void IsochronousTransfer(UsbEndpointDirection direction,
                           uint8 endpoint,
                           net::IOBuffer* buffer,
                           size_t length,
                           unsigned int packets,
                           unsigned int packet_length,
                           unsigned int timeout,
                           const UsbTransferCallback& callback) override;

  PlatformUsbDeviceHandle handle() const { return handle_; }

 protected:
  friend class UsbDeviceImpl;

  // This constructor is called by UsbDevice.
  UsbDeviceHandleImpl(scoped_refptr<UsbContext> context,
                      scoped_refptr<UsbDeviceImpl> device,
                      PlatformUsbDeviceHandle handle);

  ~UsbDeviceHandleImpl() override;

 private:
  friend class Transfer;

  class InterfaceClaimer;
  class Transfer;

  // Refresh endpoint_map_ after ClaimInterface, ReleaseInterface and
  // SetInterfaceAlternateSetting.
  void RefreshEndpointMap();

  // Look up the claimed interface by endpoint. Return NULL if the interface
  // of the endpoint is not found.
  scoped_refptr<InterfaceClaimer> GetClaimedInterfaceForEndpoint(
      unsigned char endpoint);

  // If the device's task runner is on the current thread then the transfer will
  // be submitted directly, otherwise a task to do so it posted. The callback
  // will be called on the current message loop of the thread where this
  // function was called.
  void PostOrSubmitTransfer(scoped_ptr<Transfer> transfer);

  // Submits a transfer and starts tracking it. Retains the buffer and copies
  // the completion callback until the transfer finishes, whereupon it invokes
  // the callback then releases the buffer.
  void SubmitTransfer(scoped_ptr<Transfer> transfer);

  // Invokes the callbacks associated with a given transfer, and removes it from
  // the in-flight transfer set.
  void CompleteTransfer(scoped_ptr<Transfer> transfer);

  bool GetSupportedLanguages();

  // Informs the object to drop internal references.
  void InternalClose();

  scoped_refptr<UsbDeviceImpl> device_;

  PlatformUsbDeviceHandle handle_;

  std::vector<uint16> languages_;
  std::map<uint8, base::string16> strings_;

  typedef std::map<int, scoped_refptr<InterfaceClaimer>> ClaimedInterfaceMap;
  ClaimedInterfaceMap claimed_interfaces_;

  // This set holds weak pointers to pending transfers.
  std::set<Transfer*> transfers_;

  // A map from endpoints to interfaces
  typedef std::map<int, int> EndpointMap;
  EndpointMap endpoint_map_;

  // Retain the UsbContext so that the platform context will not be destroyed
  // before this handle.
  scoped_refptr<UsbContext> context_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<UsbDeviceHandleImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UsbDeviceHandleImpl);
};

}  // namespace device

#endif  // DEVICE_USB_USB_DEVICE_HANDLE_IMPL_H_

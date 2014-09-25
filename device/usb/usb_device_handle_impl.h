// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_USB_DEVICE_HANDLE_IMPL_H_
#define DEVICE_USB_USB_DEVICE_HANDLE_IMPL_H_

#include <map>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
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
  virtual scoped_refptr<UsbDevice> GetDevice() const OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual bool ClaimInterface(int interface_number) OVERRIDE;
  virtual bool ReleaseInterface(int interface_number) OVERRIDE;
  virtual bool SetInterfaceAlternateSetting(int interface_number,
                                            int alternate_setting) OVERRIDE;
  virtual bool ResetDevice() OVERRIDE;
  virtual bool GetStringDescriptor(uint8 string_id,
                                   base::string16* string) OVERRIDE;

  virtual void ControlTransfer(UsbEndpointDirection direction,
                               TransferRequestType request_type,
                               TransferRecipient recipient,
                               uint8 request,
                               uint16 value,
                               uint16 index,
                               net::IOBuffer* buffer,
                               size_t length,
                               unsigned int timeout,
                               const UsbTransferCallback& callback) OVERRIDE;

  virtual void BulkTransfer(UsbEndpointDirection direction,
                            uint8 endpoint,
                            net::IOBuffer* buffer,
                            size_t length,
                            unsigned int timeout,
                            const UsbTransferCallback& callback) OVERRIDE;

  virtual void InterruptTransfer(UsbEndpointDirection direction,
                                 uint8 endpoint,
                                 net::IOBuffer* buffer,
                                 size_t length,
                                 unsigned int timeout,
                                 const UsbTransferCallback& callback) OVERRIDE;

  virtual void IsochronousTransfer(
      UsbEndpointDirection direction,
      uint8 endpoint,
      net::IOBuffer* buffer,
      size_t length,
      unsigned int packets,
      unsigned int packet_length,
      unsigned int timeout,
      const UsbTransferCallback& callback) OVERRIDE;

  PlatformUsbDeviceHandle handle() const { return handle_; }

 protected:
  friend class UsbDeviceImpl;

  // This constructor is called by UsbDevice.
  UsbDeviceHandleImpl(scoped_refptr<UsbContext> context,
                      UsbDeviceImpl* device,
                      PlatformUsbDeviceHandle handle,
                      const UsbConfigDescriptor& config);

  virtual ~UsbDeviceHandleImpl();

 private:
  class InterfaceClaimer;
  struct Transfer;

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
  void PostOrSubmitTransfer(PlatformUsbTransferHandle handle,
                            UsbTransferType transfer_type,
                            net::IOBuffer* buffer,
                            size_t length,
                            const UsbTransferCallback& callback);

  // Submits a transfer and starts tracking it. Retains the buffer and copies
  // the completion callback until the transfer finishes, whereupon it invokes
  // the callback then releases the buffer.
  void SubmitTransfer(PlatformUsbTransferHandle handle,
                      UsbTransferType transfer_type,
                      net::IOBuffer* buffer,
                      const size_t length,
                      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                      const UsbTransferCallback& callback);

  static void LIBUSB_CALL
      PlatformTransferCallback(PlatformUsbTransferHandle handle);

  // Invokes the callbacks associated with a given transfer, and removes it from
  // the in-flight transfer set.
  void CompleteTransfer(PlatformUsbTransferHandle transfer);

  bool GetSupportedLanguages();

  // Informs the object to drop internal references.
  void InternalClose();

  UsbDeviceImpl* device_;

  PlatformUsbDeviceHandle handle_;

  const UsbConfigDescriptor& config_;

  std::vector<uint16> languages_;
  std::map<uint8, base::string16> strings_;

  typedef std::map<int, scoped_refptr<InterfaceClaimer> > ClaimedInterfaceMap;
  ClaimedInterfaceMap claimed_interfaces_;

  typedef std::map<PlatformUsbTransferHandle, Transfer> TransferMap;
  TransferMap transfers_;

  // A map from endpoints to interfaces
  typedef std::map<int, int> EndpointMap;
  EndpointMap endpoint_map_;

  // Retain the UsbContext so that the platform context will not be destroyed
  // before this handle.
  scoped_refptr<UsbContext> context_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(UsbDeviceHandleImpl);
};

}  // namespace device

#endif  // DEVICE_USB_USB_DEVICE_HANDLE_IMPL_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USB_SERVICE_USB_DEVICE_HANDLE_IMPL_H_
#define COMPONENTS_USB_SERVICE_USB_DEVICE_HANDLE_IMPL_H_

#include <map>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "base/threading/thread_checker.h"
#include "components/usb_service/usb_device_handle.h"
#include "components/usb_service/usb_interface.h"
#include "net/base/io_buffer.h"

struct libusb_device_handle;
struct libusb_iso_packet_descriptor;
struct libusb_transfer;

namespace base {
class MessageLoopProxy;
}

namespace usb_service {

class UsbContext;
class UsbConfigDescriptor;
class UsbDeviceImpl;

typedef libusb_device_handle* PlatformUsbDeviceHandle;
typedef libusb_iso_packet_descriptor* PlatformUsbIsoPacketDescriptor;
typedef libusb_transfer* PlatformUsbTransferHandle;

// UsbDeviceHandle class provides basic I/O related functionalities.
class UsbDeviceHandleImpl : public UsbDeviceHandle {
 public:
  virtual scoped_refptr<UsbDevice> GetDevice() const OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual bool ClaimInterface(const int interface_number) OVERRIDE;
  virtual bool ReleaseInterface(const int interface_number) OVERRIDE;
  virtual bool SetInterfaceAlternateSetting(
      const int interface_number,
      const int alternate_setting) OVERRIDE;
  virtual bool ResetDevice() OVERRIDE;
  virtual bool GetManufacturer(base::string16* manufacturer) OVERRIDE;
  virtual bool GetProduct(base::string16* product) OVERRIDE;
  virtual bool GetSerial(base::string16* serial) OVERRIDE;
  virtual void ControlTransfer(const UsbEndpointDirection direction,
                               const TransferRequestType request_type,
                               const TransferRecipient recipient,
                               const uint8 request,
                               const uint16 value,
                               const uint16 index,
                               net::IOBuffer* buffer,
                               const size_t length,
                               const unsigned int timeout,
                               const UsbTransferCallback& callback) OVERRIDE;

  virtual void BulkTransfer(const UsbEndpointDirection direction,
                            const uint8 endpoint,
                            net::IOBuffer* buffer,
                            const size_t length,
                            const unsigned int timeout,
                            const UsbTransferCallback& callback) OVERRIDE;

  virtual void InterruptTransfer(const UsbEndpointDirection direction,
                                 const uint8 endpoint,
                                 net::IOBuffer* buffer,
                                 const size_t length,
                                 const unsigned int timeout,
                                 const UsbTransferCallback& callback) OVERRIDE;

  virtual void IsochronousTransfer(
      const UsbEndpointDirection direction,
      const uint8 endpoint,
      net::IOBuffer* buffer,
      const size_t length,
      const unsigned int packets,
      const unsigned int packet_length,
      const unsigned int timeout,
      const UsbTransferCallback& callback) OVERRIDE;

  PlatformUsbDeviceHandle handle() const { return handle_; }

 protected:
  friend class UsbDeviceImpl;

  // This constructor is called by UsbDevice.
  UsbDeviceHandleImpl(scoped_refptr<UsbContext> context,
                      UsbDeviceImpl* device,
                      PlatformUsbDeviceHandle handle,
                      scoped_refptr<UsbConfigDescriptor> interfaces);

  virtual ~UsbDeviceHandleImpl();

 private:
  friend void HandleTransferCompletion(PlatformUsbTransferHandle handle);

  class InterfaceClaimer;
  struct Transfer;

  // Refresh endpoint_map_ after ClaimInterface, ReleaseInterface and
  // SetInterfaceAlternateSetting.
  void RefreshEndpointMap();

  // Look up the claimed interface by endpoint. Return NULL if the interface
  // of the endpoint is not found.
  scoped_refptr<InterfaceClaimer> GetClaimedInterfaceForEndpoint(
      unsigned char endpoint);

  // Submits a transfer and starts tracking it. Retains the buffer and copies
  // the completion callback until the transfer finishes, whereupon it invokes
  // the callback then releases the buffer.
  void SubmitTransfer(PlatformUsbTransferHandle handle,
                      UsbTransferType transfer_type,
                      net::IOBuffer* buffer,
                      const size_t length,
                      scoped_refptr<base::MessageLoopProxy> message_loop_proxy,
                      const UsbTransferCallback& callback);

  // Invokes the callbacks associated with a given transfer, and removes it from
  // the in-flight transfer set.
  void TransferComplete(PlatformUsbTransferHandle transfer);

  bool GetSupportedLanguages();
  bool GetStringDescriptor(uint8 string_id, base::string16* string);

  // Informs the object to drop internal references.
  void InternalClose();

  UsbDeviceImpl* device_;

  PlatformUsbDeviceHandle handle_;

  scoped_refptr<UsbConfigDescriptor> interfaces_;

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

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(UsbDeviceHandleImpl);
};

}  // namespace usb_service

#endif  // COMPONENTS_USB_SERVICE_USB_DEVICE_HANDLE_IMPL_H_

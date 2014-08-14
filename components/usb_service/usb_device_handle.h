// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USB_SERVICE_USB_DEVICE_HANDLE_H_
#define COMPONENTS_USB_SERVICE_USB_DEVICE_HANDLE_H_

#include <map>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "base/threading/thread_checker.h"
#include "components/usb_service/usb_interface.h"
#include "components/usb_service/usb_service_export.h"
#include "net/base/io_buffer.h"

namespace usb_service {

class UsbDevice;

enum UsbTransferStatus {
  USB_TRANSFER_COMPLETED = 0,
  USB_TRANSFER_ERROR,
  USB_TRANSFER_TIMEOUT,
  USB_TRANSFER_CANCELLED,
  USB_TRANSFER_STALLED,
  USB_TRANSFER_DISCONNECT,
  USB_TRANSFER_OVERFLOW,
  USB_TRANSFER_LENGTH_SHORT,
};

typedef base::Callback<
    void(UsbTransferStatus, scoped_refptr<net::IOBuffer>, size_t)>
    UsbTransferCallback;

// UsbDeviceHandle class provides basic I/O related functionalities.
class USB_SERVICE_EXPORT UsbDeviceHandle
    : public base::RefCountedThreadSafe<UsbDeviceHandle> {
 public:
  enum TransferRequestType { STANDARD, CLASS, VENDOR, RESERVED };
  enum TransferRecipient { DEVICE, INTERFACE, ENDPOINT, OTHER };

  virtual scoped_refptr<UsbDevice> GetDevice() const = 0;

  // Notifies UsbDevice to drop the reference of this object; cancels all the
  // flying transfers.
  // It is possible that the object has no other reference after this call. So
  // if it is called using a raw pointer, it could be invalidated.
  // The platform device handle will be closed when UsbDeviceHandle destructs.
  virtual void Close() = 0;

  // Device manipulation operations. These methods are blocking and must be
  // called on FILE thread.
  virtual bool ClaimInterface(const int interface_number) = 0;
  virtual bool ReleaseInterface(const int interface_number) = 0;
  virtual bool SetInterfaceAlternateSetting(const int interface_number,
                                            const int alternate_setting) = 0;
  virtual bool ResetDevice() = 0;
  virtual bool GetManufacturer(base::string16* manufacturer) = 0;
  virtual bool GetProduct(base::string16* product) = 0;
  virtual bool GetSerial(base::string16* serial) = 0;

  // Async IO. Can be called on any thread.
  virtual void ControlTransfer(const UsbEndpointDirection direction,
                               const TransferRequestType request_type,
                               const TransferRecipient recipient,
                               const uint8 request,
                               const uint16 value,
                               const uint16 index,
                               net::IOBuffer* buffer,
                               const size_t length,
                               const unsigned int timeout,
                               const UsbTransferCallback& callback) = 0;

  virtual void BulkTransfer(const UsbEndpointDirection direction,
                            const uint8 endpoint,
                            net::IOBuffer* buffer,
                            const size_t length,
                            const unsigned int timeout,
                            const UsbTransferCallback& callback) = 0;

  virtual void InterruptTransfer(const UsbEndpointDirection direction,
                                 const uint8 endpoint,
                                 net::IOBuffer* buffer,
                                 const size_t length,
                                 const unsigned int timeout,
                                 const UsbTransferCallback& callback) = 0;

  virtual void IsochronousTransfer(const UsbEndpointDirection direction,
                                   const uint8 endpoint,
                                   net::IOBuffer* buffer,
                                   const size_t length,
                                   const unsigned int packets,
                                   const unsigned int packet_length,
                                   const unsigned int timeout,
                                   const UsbTransferCallback& callback) = 0;

 protected:
  friend class base::RefCountedThreadSafe<UsbDeviceHandle>;

  UsbDeviceHandle() {};

  virtual ~UsbDeviceHandle() {};

  DISALLOW_COPY_AND_ASSIGN(UsbDeviceHandle);
};

}  // namespace usb_service

#endif  // COMPONENTS_USB_SERVICE_USB_DEVICE_HANDLE_H_

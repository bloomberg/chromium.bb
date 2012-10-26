// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_USB_DEVICE_H_
#define CHROME_BROWSER_USB_USB_DEVICE_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"

struct libusb_device;
struct libusb_device_handle;
struct libusb_iso_packet_descriptor;
struct libusb_transfer;

typedef libusb_device* PlatformUsbDevice;
typedef libusb_device_handle* PlatformUsbDeviceHandle;
typedef libusb_iso_packet_descriptor* PlatformUsbIsoPacketDescriptor;
typedef libusb_transfer* PlatformUsbTransferHandle;

class UsbService;

namespace net {
class IOBuffer;
}  // namespace net

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

enum UsbTransferType {
  USB_TRANSFER_CONTROL = 0,
  USB_TRANSFER_BULK,
  USB_TRANSFER_INTERRUPT,
  USB_TRANSFER_ISOCHRONOUS,
};

typedef base::Callback<void(UsbTransferStatus, scoped_refptr<net::IOBuffer>,
    size_t)> UsbTransferCallback;
typedef base::Callback<void(bool)> UsbInterfaceCallback;

// A UsbDevice wraps the platform's underlying representation of what a USB
// device actually is, and provides accessors for performing many of the
// standard USB operations.
class UsbDevice : public base::RefCounted<UsbDevice> {
 public:
  enum TransferDirection { INBOUND, OUTBOUND };
  enum TransferRequestType { STANDARD, CLASS, VENDOR, RESERVED };
  enum TransferRecipient { DEVICE, INTERFACE, ENDPOINT, OTHER };

  // Usually you will not want to directly create a UsbDevice, favoring to let
  // the UsbService take care of the logistics of getting a platform device
  // handle and handling events for it.
  UsbDevice(UsbService* service, PlatformUsbDeviceHandle handle);

  PlatformUsbDeviceHandle handle() { return handle_; }

  // Close the USB device and release the underlying platform device. |callback|
  // is invoked after the device has been closed.
  virtual void Close(const base::Callback<void()>& callback);

  virtual void ClaimInterface(const int interface_number,
                              const UsbInterfaceCallback& callback);

  virtual void ReleaseInterface(const int interface_number,
                                const UsbInterfaceCallback& callback);

  virtual void SetInterfaceAlternateSetting(
      const int interface_number,
      const int alternate_setting,
      const UsbInterfaceCallback& callback);

  virtual void ControlTransfer(const TransferDirection direction,
                               const TransferRequestType request_type,
                               const TransferRecipient recipient,
                               const uint8 request,
                               const uint16 value,
                               const uint16 index,
                               net::IOBuffer* buffer,
                               const size_t length,
                               const unsigned int timeout,
                               const UsbTransferCallback& callback);

  virtual void BulkTransfer(const TransferDirection direction,
                            const uint8 endpoint,
                            net::IOBuffer* buffer,
                            const size_t length,
                            const unsigned int timeout,
                            const UsbTransferCallback& callback);

  virtual void InterruptTransfer(const TransferDirection direction,
                                 const uint8 endpoint,
                                 net::IOBuffer* buffer,
                                 const size_t length,
                                 const unsigned int timeout,
                                 const UsbTransferCallback& callback);

  virtual void IsochronousTransfer(const TransferDirection direction,
                                   const uint8 endpoint,
                                   net::IOBuffer* buffer,
                                   const size_t length,
                                   const unsigned int packets,
                                   const unsigned int packet_length,
                                   const unsigned int timeout,
                                   const UsbTransferCallback& callback);

  // Normal code should not call this function. It is called by the platform's
  // callback mechanism in such a way that it cannot be made private. Invokes
  // the callbacks associated with a given transfer, and removes it from the
  // in-flight transfer set.
  void TransferComplete(PlatformUsbTransferHandle transfer);

 protected:
  // This constructor variant is for use in testing only.
  UsbDevice();

  friend class base::RefCounted<UsbDevice>;
  virtual ~UsbDevice();

 private:
  struct Transfer {
    Transfer();
    ~Transfer();

    UsbTransferType transfer_type;
    scoped_refptr<net::IOBuffer> buffer;
    size_t length;
    UsbTransferCallback callback;
  };

  // Checks that the device has not yet been closed.
  void CheckDevice();

  // Submits a transfer and starts tracking it. Retains the buffer and copies
  // the completion callback until the transfer finishes, whereupon it invokes
  // the callback then releases the buffer.
  void SubmitTransfer(PlatformUsbTransferHandle handle,
                      UsbTransferType transfer_type,
                      net::IOBuffer* buffer,
                      const size_t length,
                      const UsbTransferCallback& callback);

  // The UsbService isn't referenced here to prevent a dependency cycle between
  // the service and the devices. Since a service owns every device, and is
  // responsible for its destruction, there is no case where a UsbDevice can
  // have outlived its originating UsbService.
  UsbService* const service_;
  PlatformUsbDeviceHandle handle_;

  // transfers_ tracks all in-flight transfers associated with this device,
  // allowing the device to retain the buffer and callback associated with a
  // transfer until such time that it completes. It is protected by lock_.
  base::Lock lock_;
  std::map<PlatformUsbTransferHandle, Transfer> transfers_;

  DISALLOW_COPY_AND_ASSIGN(UsbDevice);
};

#endif  // CHROME_BROWSER_USB_USB_DEVICE_H_

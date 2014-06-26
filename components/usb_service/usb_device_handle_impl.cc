// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/usb_service/usb_device_handle_impl.h"

#include <algorithm>
#include <vector>

#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/synchronization/lock.h"
#include "components/usb_service/usb_context.h"
#include "components/usb_service/usb_device_impl.h"
#include "components/usb_service/usb_error.h"
#include "components/usb_service/usb_interface.h"
#include "components/usb_service/usb_service.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/libusb/src/libusb/libusb.h"

using content::BrowserThread;

namespace usb_service {

typedef libusb_device* PlatformUsbDevice;

void HandleTransferCompletion(usb_service::PlatformUsbTransferHandle transfer);

namespace {

static uint8 ConvertTransferDirection(const UsbEndpointDirection direction) {
  switch (direction) {
    case USB_DIRECTION_INBOUND:
      return LIBUSB_ENDPOINT_IN;
    case USB_DIRECTION_OUTBOUND:
      return LIBUSB_ENDPOINT_OUT;
    default:
      NOTREACHED();
      return LIBUSB_ENDPOINT_IN;
  }
}

static uint8 CreateRequestType(
    const UsbEndpointDirection direction,
    const UsbDeviceHandle::TransferRequestType request_type,
    const UsbDeviceHandle::TransferRecipient recipient) {
  uint8 result = ConvertTransferDirection(direction);

  switch (request_type) {
    case UsbDeviceHandle::STANDARD:
      result |= LIBUSB_REQUEST_TYPE_STANDARD;
      break;
    case UsbDeviceHandle::CLASS:
      result |= LIBUSB_REQUEST_TYPE_CLASS;
      break;
    case UsbDeviceHandle::VENDOR:
      result |= LIBUSB_REQUEST_TYPE_VENDOR;
      break;
    case UsbDeviceHandle::RESERVED:
      result |= LIBUSB_REQUEST_TYPE_RESERVED;
      break;
  }

  switch (recipient) {
    case UsbDeviceHandle::DEVICE:
      result |= LIBUSB_RECIPIENT_DEVICE;
      break;
    case UsbDeviceHandle::INTERFACE:
      result |= LIBUSB_RECIPIENT_INTERFACE;
      break;
    case UsbDeviceHandle::ENDPOINT:
      result |= LIBUSB_RECIPIENT_ENDPOINT;
      break;
    case UsbDeviceHandle::OTHER:
      result |= LIBUSB_RECIPIENT_OTHER;
      break;
  }

  return result;
}

static UsbTransferStatus ConvertTransferStatus(
    const libusb_transfer_status status) {
  switch (status) {
    case LIBUSB_TRANSFER_COMPLETED:
      return USB_TRANSFER_COMPLETED;
    case LIBUSB_TRANSFER_ERROR:
      return USB_TRANSFER_ERROR;
    case LIBUSB_TRANSFER_TIMED_OUT:
      return USB_TRANSFER_TIMEOUT;
    case LIBUSB_TRANSFER_STALL:
      return USB_TRANSFER_STALLED;
    case LIBUSB_TRANSFER_NO_DEVICE:
      return USB_TRANSFER_DISCONNECT;
    case LIBUSB_TRANSFER_OVERFLOW:
      return USB_TRANSFER_OVERFLOW;
    case LIBUSB_TRANSFER_CANCELLED:
      return USB_TRANSFER_CANCELLED;
    default:
      NOTREACHED();
      return USB_TRANSFER_ERROR;
  }
}

static void LIBUSB_CALL
PlatformTransferCompletionCallback(PlatformUsbTransferHandle transfer) {
  BrowserThread::PostTask(BrowserThread::FILE,
                          FROM_HERE,
                          base::Bind(HandleTransferCompletion, transfer));
}

}  // namespace

void HandleTransferCompletion(PlatformUsbTransferHandle transfer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  UsbDeviceHandleImpl* const device_handle =
      reinterpret_cast<UsbDeviceHandleImpl*>(transfer->user_data);
  CHECK(device_handle) << "Device handle is closed before transfer finishes.";
  device_handle->TransferComplete(transfer);
  libusb_free_transfer(transfer);
}

class UsbDeviceHandleImpl::InterfaceClaimer
    : public base::RefCountedThreadSafe<UsbDeviceHandleImpl::InterfaceClaimer> {
 public:
  InterfaceClaimer(const scoped_refptr<UsbDeviceHandleImpl> handle,
                   const int interface_number);

  bool Claim() const;

  int alternate_setting() const { return alternate_setting_; }
  void set_alternate_setting(const int alternate_setting) {
    alternate_setting_ = alternate_setting;
  }

 private:
  friend class UsbDevice;
  friend class base::RefCountedThreadSafe<InterfaceClaimer>;
  ~InterfaceClaimer();

  const scoped_refptr<UsbDeviceHandleImpl> handle_;
  const int interface_number_;
  int alternate_setting_;

  DISALLOW_COPY_AND_ASSIGN(InterfaceClaimer);
};

UsbDeviceHandleImpl::InterfaceClaimer::InterfaceClaimer(
    const scoped_refptr<UsbDeviceHandleImpl> handle,
    const int interface_number)
    : handle_(handle),
      interface_number_(interface_number),
      alternate_setting_(0) {
}

UsbDeviceHandleImpl::InterfaceClaimer::~InterfaceClaimer() {
  libusb_release_interface(handle_->handle(), interface_number_);
}

bool UsbDeviceHandleImpl::InterfaceClaimer::Claim() const {
  const int rv = libusb_claim_interface(handle_->handle(), interface_number_);
  if (rv != LIBUSB_SUCCESS) {
    VLOG(1) << "Failed to claim interface: " << ConvertErrorToString(rv);
  }
  return rv == LIBUSB_SUCCESS;
}

struct UsbDeviceHandleImpl::Transfer {
  Transfer();
  ~Transfer();

  UsbTransferType transfer_type;
  scoped_refptr<net::IOBuffer> buffer;
  scoped_refptr<UsbDeviceHandleImpl::InterfaceClaimer> claimed_interface;
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy;
  size_t length;
  UsbTransferCallback callback;
};

UsbDeviceHandleImpl::Transfer::Transfer()
    : transfer_type(USB_TRANSFER_CONTROL), length(0) {
}

UsbDeviceHandleImpl::Transfer::~Transfer() {
}

UsbDeviceHandleImpl::UsbDeviceHandleImpl(
    scoped_refptr<UsbContext> context,
    UsbDeviceImpl* device,
    PlatformUsbDeviceHandle handle,
    scoped_refptr<UsbConfigDescriptor> interfaces)
    : device_(device),
      handle_(handle),
      interfaces_(interfaces),
      context_(context) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(handle) << "Cannot create device with NULL handle.";
  DCHECK(interfaces_) << "Unabled to list interfaces";
}

UsbDeviceHandleImpl::~UsbDeviceHandleImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());

  libusb_close(handle_);
  handle_ = NULL;
}

scoped_refptr<UsbDevice> UsbDeviceHandleImpl::GetDevice() const {
  return static_cast<UsbDevice*>(device_);
}

void UsbDeviceHandleImpl::Close() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (device_)
    device_->Close(this);
}

void UsbDeviceHandleImpl::TransferComplete(PlatformUsbTransferHandle handle) {
  DCHECK(ContainsKey(transfers_, handle)) << "Missing transfer completed";

  Transfer transfer = transfers_[handle];
  transfers_.erase(handle);

  DCHECK_GE(handle->actual_length, 0) << "Negative actual length received";
  size_t actual_length =
      static_cast<size_t>(std::max(handle->actual_length, 0));

  DCHECK(transfer.length >= actual_length)
      << "data too big for our buffer (libusb failure?)";

  scoped_refptr<net::IOBuffer> buffer = transfer.buffer;
  switch (transfer.transfer_type) {
    case USB_TRANSFER_CONTROL:
      // If the transfer is a control transfer we do not expose the control
      // setup header to the caller. This logic strips off the header if
      // present before invoking the callback provided with the transfer.
      if (actual_length > 0) {
        CHECK(transfer.length >= LIBUSB_CONTROL_SETUP_SIZE)
            << "buffer was not correctly set: too small for the control header";

        if (transfer.length >= (LIBUSB_CONTROL_SETUP_SIZE + actual_length)) {
          // If the payload is zero bytes long, pad out the allocated buffer
          // size to one byte so that an IOBuffer of that size can be allocated.
          scoped_refptr<net::IOBuffer> resized_buffer =
              new net::IOBuffer(static_cast<int>(
                  std::max(actual_length, static_cast<size_t>(1))));
          memcpy(resized_buffer->data(),
                 buffer->data() + LIBUSB_CONTROL_SETUP_SIZE,
                 actual_length);
          buffer = resized_buffer;
        }
      }
      break;

    case USB_TRANSFER_ISOCHRONOUS:
      // Isochronous replies might carry data in the different isoc packets even
      // if the transfer actual_data value is zero. Furthermore, not all of the
      // received packets might contain data, so we need to calculate how many
      // data bytes we are effectively providing and pack the results.
      if (actual_length == 0) {
        size_t packet_buffer_start = 0;
        for (int i = 0; i < handle->num_iso_packets; ++i) {
          PlatformUsbIsoPacketDescriptor packet = &handle->iso_packet_desc[i];
          if (packet->actual_length > 0) {
            // We don't need to copy as long as all packets until now provide
            // all the data the packet can hold.
            if (actual_length < packet_buffer_start) {
              CHECK(packet_buffer_start + packet->actual_length <=
                    transfer.length);
              memmove(buffer->data() + actual_length,
                      buffer->data() + packet_buffer_start,
                      packet->actual_length);
            }
            actual_length += packet->actual_length;
          }

          packet_buffer_start += packet->length;
        }
      }
      break;

    case USB_TRANSFER_BULK:
    case USB_TRANSFER_INTERRUPT:
      break;

    default:
      NOTREACHED() << "Invalid usb transfer type";
      break;
  }

  transfer.message_loop_proxy->PostTask(
      FROM_HERE,
      base::Bind(transfer.callback,
                 ConvertTransferStatus(handle->status),
                 buffer,
                 actual_length));

  // Must release interface first before actually delete this.
  transfer.claimed_interface = NULL;
}

bool UsbDeviceHandleImpl::ClaimInterface(const int interface_number) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!device_)
    return false;
  if (ContainsKey(claimed_interfaces_, interface_number))
    return true;

  scoped_refptr<InterfaceClaimer> claimer =
      new InterfaceClaimer(this, interface_number);

  if (claimer->Claim()) {
    claimed_interfaces_[interface_number] = claimer;
    RefreshEndpointMap();
    return true;
  }
  return false;
}

bool UsbDeviceHandleImpl::ReleaseInterface(const int interface_number) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!device_)
    return false;
  if (!ContainsKey(claimed_interfaces_, interface_number))
    return false;

  // Cancel all the transfers on that interface.
  InterfaceClaimer* interface_claimer =
      claimed_interfaces_[interface_number].get();
  for (TransferMap::iterator it = transfers_.begin(); it != transfers_.end();
       ++it) {
    if (it->second.claimed_interface.get() == interface_claimer)
      libusb_cancel_transfer(it->first);
  }
  claimed_interfaces_.erase(interface_number);

  RefreshEndpointMap();
  return true;
}

bool UsbDeviceHandleImpl::SetInterfaceAlternateSetting(
    const int interface_number,
    const int alternate_setting) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!device_)
    return false;
  if (!ContainsKey(claimed_interfaces_, interface_number))
    return false;
  const int rv = libusb_set_interface_alt_setting(
      handle_, interface_number, alternate_setting);
  if (rv == LIBUSB_SUCCESS) {
    claimed_interfaces_[interface_number]->set_alternate_setting(
        alternate_setting);
    RefreshEndpointMap();
  } else {
    VLOG(1) << "Failed to set interface (" << interface_number << ", "
            << alternate_setting << "): " << ConvertErrorToString(rv);
  }
  return rv == LIBUSB_SUCCESS;
}

bool UsbDeviceHandleImpl::ResetDevice() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!device_)
    return false;

  const int rv = libusb_reset_device(handle_);
  if (rv != LIBUSB_SUCCESS) {
    VLOG(1) << "Failed to reset device: " << ConvertErrorToString(rv);
  }
  return rv == LIBUSB_SUCCESS;
}

bool UsbDeviceHandleImpl::GetSerial(base::string16* serial) {
  DCHECK(thread_checker_.CalledOnValidThread());
  PlatformUsbDevice device = libusb_get_device(handle_);
  libusb_device_descriptor desc;

  const int rv = libusb_get_device_descriptor(device, &desc);
  if (rv != LIBUSB_SUCCESS) {
    VLOG(1) << "Failed to read device descriptor: " << ConvertErrorToString(rv);
    return false;
  }

  if (desc.iSerialNumber == 0)
    return false;

  // Getting supported language ID.
  uint16 langid[128] = {0};

  int size =
      libusb_get_string_descriptor(handle_,
                                   0,
                                   0,
                                   reinterpret_cast<unsigned char*>(&langid[0]),
                                   sizeof(langid));
  if (size < 0) {
    VLOG(1) << "Failed to get language IDs: " << ConvertErrorToString(size);
    return false;
  }

  int language_count = (size - 2) / 2;

  for (int i = 1; i <= language_count; ++i) {
    // Get the string using language ID.
    base::char16 text[256] = {0};
    size =
        libusb_get_string_descriptor(handle_,
                                     desc.iSerialNumber,
                                     langid[i],
                                     reinterpret_cast<unsigned char*>(&text[0]),
                                     sizeof(text));
    if (size < 0) {
      VLOG(1) << "Failed to get serial number (langid " << langid[i] << "): "
              << ConvertErrorToString(size);
      continue;
    }
    if (size <= 2)
      continue;
    if ((text[0] >> 8) != LIBUSB_DT_STRING)
      continue;
    if ((text[0] & 255) > size)
      continue;

    size = size / 2 - 1;
    *serial = base::string16(text + 1, size);
    return true;
  }
  return false;
}

void UsbDeviceHandleImpl::ControlTransfer(
    const UsbEndpointDirection direction,
    const TransferRequestType request_type,
    const TransferRecipient recipient,
    const uint8 request,
    const uint16 value,
    const uint16 index,
    net::IOBuffer* buffer,
    const size_t length,
    const unsigned int timeout,
    const UsbTransferCallback& callback) {
  if (!device_) {
    callback.Run(USB_TRANSFER_DISCONNECT, buffer, 0);
    return;
  }

  const size_t resized_length = LIBUSB_CONTROL_SETUP_SIZE + length;
  scoped_refptr<net::IOBuffer> resized_buffer(
      new net::IOBufferWithSize(static_cast<int>(resized_length)));
  if (!resized_buffer) {
    callback.Run(USB_TRANSFER_ERROR, buffer, 0);
    return;
  }
  memcpy(resized_buffer->data() + LIBUSB_CONTROL_SETUP_SIZE,
         buffer->data(),
         static_cast<int>(length));

  PlatformUsbTransferHandle const transfer = libusb_alloc_transfer(0);
  const uint8 converted_type =
      CreateRequestType(direction, request_type, recipient);
  libusb_fill_control_setup(reinterpret_cast<uint8*>(resized_buffer->data()),
                            converted_type,
                            request,
                            value,
                            index,
                            static_cast<int16>(length));
  libusb_fill_control_transfer(transfer,
                               handle_,
                               reinterpret_cast<uint8*>(resized_buffer->data()),
                               PlatformTransferCompletionCallback,
                               this,
                               timeout);

  BrowserThread::PostTask(BrowserThread::FILE,
                          FROM_HERE,
                          base::Bind(&UsbDeviceHandleImpl::SubmitTransfer,
                                     this,
                                     transfer,
                                     USB_TRANSFER_CONTROL,
                                     resized_buffer,
                                     resized_length,
                                     base::MessageLoopProxy::current(),
                                     callback));
}

void UsbDeviceHandleImpl::BulkTransfer(const UsbEndpointDirection direction,
                                       const uint8 endpoint,
                                       net::IOBuffer* buffer,
                                       const size_t length,
                                       const unsigned int timeout,
                                       const UsbTransferCallback& callback) {
  if (!device_) {
    callback.Run(USB_TRANSFER_DISCONNECT, buffer, 0);
    return;
  }

  PlatformUsbTransferHandle const transfer = libusb_alloc_transfer(0);
  const uint8 new_endpoint = ConvertTransferDirection(direction) | endpoint;
  libusb_fill_bulk_transfer(transfer,
                            handle_,
                            new_endpoint,
                            reinterpret_cast<uint8*>(buffer->data()),
                            static_cast<int>(length),
                            PlatformTransferCompletionCallback,
                            this,
                            timeout);

  BrowserThread::PostTask(BrowserThread::FILE,
                          FROM_HERE,
                          base::Bind(&UsbDeviceHandleImpl::SubmitTransfer,
                                     this,
                                     transfer,
                                     USB_TRANSFER_BULK,
                                     make_scoped_refptr(buffer),
                                     length,
                                     base::MessageLoopProxy::current(),
                                     callback));
}

void UsbDeviceHandleImpl::InterruptTransfer(
    const UsbEndpointDirection direction,
    const uint8 endpoint,
    net::IOBuffer* buffer,
    const size_t length,
    const unsigned int timeout,
    const UsbTransferCallback& callback) {
  if (!device_) {
    callback.Run(USB_TRANSFER_DISCONNECT, buffer, 0);
    return;
  }

  PlatformUsbTransferHandle const transfer = libusb_alloc_transfer(0);
  const uint8 new_endpoint = ConvertTransferDirection(direction) | endpoint;
  libusb_fill_interrupt_transfer(transfer,
                                 handle_,
                                 new_endpoint,
                                 reinterpret_cast<uint8*>(buffer->data()),
                                 static_cast<int>(length),
                                 PlatformTransferCompletionCallback,
                                 this,
                                 timeout);
  BrowserThread::PostTask(BrowserThread::FILE,
                          FROM_HERE,
                          base::Bind(&UsbDeviceHandleImpl::SubmitTransfer,
                                     this,
                                     transfer,
                                     USB_TRANSFER_INTERRUPT,
                                     make_scoped_refptr(buffer),
                                     length,
                                     base::MessageLoopProxy::current(),
                                     callback));
}

void UsbDeviceHandleImpl::IsochronousTransfer(
    const UsbEndpointDirection direction,
    const uint8 endpoint,
    net::IOBuffer* buffer,
    const size_t length,
    const unsigned int packets,
    const unsigned int packet_length,
    const unsigned int timeout,
    const UsbTransferCallback& callback) {
  if (!device_) {
    callback.Run(USB_TRANSFER_DISCONNECT, buffer, 0);
    return;
  }

  const uint64 total_length = packets * packet_length;
  CHECK(packets <= length && total_length <= length)
      << "transfer length is too small";

  PlatformUsbTransferHandle const transfer = libusb_alloc_transfer(packets);
  const uint8 new_endpoint = ConvertTransferDirection(direction) | endpoint;
  libusb_fill_iso_transfer(transfer,
                           handle_,
                           new_endpoint,
                           reinterpret_cast<uint8*>(buffer->data()),
                           static_cast<int>(length),
                           packets,
                           PlatformTransferCompletionCallback,
                           this,
                           timeout);
  libusb_set_iso_packet_lengths(transfer, packet_length);

  BrowserThread::PostTask(BrowserThread::FILE,
                          FROM_HERE,
                          base::Bind(&UsbDeviceHandleImpl::SubmitTransfer,
                                     this,
                                     transfer,
                                     USB_TRANSFER_ISOCHRONOUS,
                                     make_scoped_refptr(buffer),
                                     length,
                                     base::MessageLoopProxy::current(),
                                     callback));
}

void UsbDeviceHandleImpl::RefreshEndpointMap() {
  DCHECK(thread_checker_.CalledOnValidThread());
  endpoint_map_.clear();
  for (ClaimedInterfaceMap::iterator it = claimed_interfaces_.begin();
       it != claimed_interfaces_.end();
       ++it) {
    scoped_refptr<const UsbInterfaceAltSettingDescriptor> interface_desc =
        interfaces_->GetInterface(it->first)
            ->GetAltSetting(it->second->alternate_setting());
    for (size_t i = 0; i < interface_desc->GetNumEndpoints(); i++) {
      scoped_refptr<const UsbEndpointDescriptor> endpoint =
          interface_desc->GetEndpoint(i);
      endpoint_map_[endpoint->GetAddress()] = it->first;
    }
  }
}

scoped_refptr<UsbDeviceHandleImpl::InterfaceClaimer>
UsbDeviceHandleImpl::GetClaimedInterfaceForEndpoint(unsigned char endpoint) {
  unsigned char address = endpoint & LIBUSB_ENDPOINT_ADDRESS_MASK;
  if (ContainsKey(endpoint_map_, address))
    return claimed_interfaces_[endpoint_map_[address]];
  return NULL;
}

void UsbDeviceHandleImpl::SubmitTransfer(
    PlatformUsbTransferHandle handle,
    UsbTransferType transfer_type,
    net::IOBuffer* buffer,
    const size_t length,
    scoped_refptr<base::MessageLoopProxy> message_loop_proxy,
    const UsbTransferCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!device_) {
    message_loop_proxy->PostTask(
        FROM_HERE,
        base::Bind(
            callback, USB_TRANSFER_DISCONNECT, make_scoped_refptr(buffer), 0));
  }

  Transfer transfer;
  transfer.transfer_type = transfer_type;
  transfer.buffer = buffer;
  transfer.length = length;
  transfer.callback = callback;
  transfer.message_loop_proxy = message_loop_proxy;

  // It's OK for this method to return NULL. libusb_submit_transfer will fail if
  // it requires an interface we didn't claim.
  transfer.claimed_interface = GetClaimedInterfaceForEndpoint(handle->endpoint);

  const int rv = libusb_submit_transfer(handle);
  if (rv == LIBUSB_SUCCESS) {
    transfers_[handle] = transfer;
  } else {
    VLOG(1) << "Failed to submit transfer: " << ConvertErrorToString(rv);
    message_loop_proxy->PostTask(
        FROM_HERE,
        base::Bind(
            callback, USB_TRANSFER_ERROR, make_scoped_refptr(buffer), 0));
  }
}

void UsbDeviceHandleImpl::InternalClose() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!device_)
    return;

  // Cancel all the transfers.
  for (TransferMap::iterator it = transfers_.begin(); it != transfers_.end();
       ++it) {
    // The callback will be called some time later.
    libusb_cancel_transfer(it->first);
  }

  // Attempt-release all the interfaces.
  // It will be retained until the transfer cancellation is finished.
  claimed_interfaces_.clear();

  // Cannot close device handle here. Need to wait for libusb_cancel_transfer to
  // finish.
  device_ = NULL;
}

}  // namespace usb_service

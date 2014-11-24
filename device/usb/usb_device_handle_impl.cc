// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_device_handle_impl.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/synchronization/lock.h"
#include "base/thread_task_runner_handle.h"
#include "device/usb/usb_context.h"
#include "device/usb/usb_descriptors.h"
#include "device/usb/usb_device_impl.h"
#include "device/usb/usb_error.h"
#include "device/usb/usb_service.h"
#include "third_party/libusb/src/libusb/libusb.h"

namespace device {

typedef libusb_device* PlatformUsbDevice;

void HandleTransferCompletion(PlatformUsbTransferHandle transfer);

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

}  // namespace

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
    VLOG(1) << "Failed to claim interface: "
            << ConvertPlatformUsbErrorToString(rv);
  }
  return rv == LIBUSB_SUCCESS;
}

struct UsbDeviceHandleImpl::Transfer {
  Transfer();
  ~Transfer();

  void Complete(UsbTransferStatus status, size_t bytes_transferred);

  UsbTransferType transfer_type;
  scoped_refptr<net::IOBuffer> buffer;
  scoped_refptr<UsbDeviceHandleImpl::InterfaceClaimer> claimed_interface;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner;
  size_t length;
  UsbTransferCallback callback;
};

UsbDeviceHandleImpl::Transfer::Transfer()
    : transfer_type(USB_TRANSFER_CONTROL), length(0) {
}

UsbDeviceHandleImpl::Transfer::~Transfer() {
}

void UsbDeviceHandleImpl::Transfer::Complete(UsbTransferStatus status,
                                             size_t bytes_transferred) {
  if (task_runner->RunsTasksOnCurrentThread()) {
    callback.Run(status, buffer, bytes_transferred);
  } else {
    task_runner->PostTask(
        FROM_HERE, base::Bind(callback, status, buffer, bytes_transferred));
  }
}

UsbDeviceHandleImpl::UsbDeviceHandleImpl(scoped_refptr<UsbContext> context,
                                         UsbDeviceImpl* device,
                                         PlatformUsbDeviceHandle handle,
                                         const UsbConfigDescriptor& config)
    : device_(device),
      handle_(handle),
      config_(config),
      context_(context),
      task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  DCHECK(handle) << "Cannot create device with NULL handle.";
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
            << alternate_setting
            << "): " << ConvertPlatformUsbErrorToString(rv);
  }
  return rv == LIBUSB_SUCCESS;
}

bool UsbDeviceHandleImpl::ResetDevice() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!device_)
    return false;

  const int rv = libusb_reset_device(handle_);
  if (rv != LIBUSB_SUCCESS) {
    VLOG(1) << "Failed to reset device: "
            << ConvertPlatformUsbErrorToString(rv);
  }
  return rv == LIBUSB_SUCCESS;
}

bool UsbDeviceHandleImpl::GetStringDescriptor(uint8 string_id,
                                              base::string16* string) {
  if (!GetSupportedLanguages()) {
    return false;
  }

  std::map<uint8, base::string16>::const_iterator it = strings_.find(string_id);
  if (it != strings_.end()) {
    *string = it->second;
    return true;
  }

  for (size_t i = 0; i < languages_.size(); ++i) {
    // Get the string using language ID.
    uint16 language_id = languages_[i];
    // The 1-byte length field limits the descriptor to 256-bytes (128 char16s).
    base::char16 text[128];
    int size =
        libusb_get_string_descriptor(handle_,
                                     string_id,
                                     language_id,
                                     reinterpret_cast<unsigned char*>(&text[0]),
                                     sizeof(text));
    if (size < 0) {
      VLOG(1) << "Failed to get string descriptor " << string_id << " (langid "
              << language_id << "): " << ConvertPlatformUsbErrorToString(size);
      continue;
    } else if (size < 2) {
      VLOG(1) << "String descriptor " << string_id << " (langid " << language_id
              << ") has no header.";
      continue;
      // The first 2 bytes of the descriptor are the total length and type tag.
    } else if ((text[0] & 0xff) != size) {
      VLOG(1) << "String descriptor " << string_id << " (langid " << language_id
              << ") size mismatch: " << (text[0] & 0xff) << " != " << size;
      continue;
    } else if ((text[0] >> 8) != LIBUSB_DT_STRING) {
      VLOG(1) << "String descriptor " << string_id << " (langid " << language_id
              << ") is not a string descriptor.";
      continue;
    }

    *string = base::string16(text + 1, (size - 2) / 2);
    strings_[string_id] = *string;
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
  if (!resized_buffer.get()) {
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
                               &UsbDeviceHandleImpl::PlatformTransferCallback,
                               this,
                               timeout);

  PostOrSubmitTransfer(transfer,
                       USB_TRANSFER_CONTROL,
                       resized_buffer.get(),
                       resized_length,
                       callback);
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
                            &UsbDeviceHandleImpl::PlatformTransferCallback,
                            this,
                            timeout);

  PostOrSubmitTransfer(transfer, USB_TRANSFER_BULK, buffer, length, callback);
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
                                 &UsbDeviceHandleImpl::PlatformTransferCallback,
                                 this,
                                 timeout);

  PostOrSubmitTransfer(
      transfer, USB_TRANSFER_INTERRUPT, buffer, length, callback);
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
                           &UsbDeviceHandleImpl::PlatformTransferCallback,
                           this,
                           timeout);
  libusb_set_iso_packet_lengths(transfer, packet_length);

  PostOrSubmitTransfer(
      transfer, USB_TRANSFER_ISOCHRONOUS, buffer, length, callback);
}

void UsbDeviceHandleImpl::RefreshEndpointMap() {
  DCHECK(thread_checker_.CalledOnValidThread());
  endpoint_map_.clear();
  for (ClaimedInterfaceMap::iterator claimedIt = claimed_interfaces_.begin();
       claimedIt != claimed_interfaces_.end();
       ++claimedIt) {
    for (UsbInterfaceDescriptor::Iterator ifaceIt = config_.interfaces.begin();
         ifaceIt != config_.interfaces.end();
         ++ifaceIt) {
      if (ifaceIt->interface_number == claimedIt->first &&
          ifaceIt->alternate_setting ==
              claimedIt->second->alternate_setting()) {
        for (UsbEndpointDescriptor::Iterator endpointIt =
                 ifaceIt->endpoints.begin();
             endpointIt != ifaceIt->endpoints.end();
             ++endpointIt) {
          endpoint_map_[endpointIt->address] = claimedIt->first;
        }
        break;
      }
    }
  }
}

scoped_refptr<UsbDeviceHandleImpl::InterfaceClaimer>
UsbDeviceHandleImpl::GetClaimedInterfaceForEndpoint(unsigned char endpoint) {
  if (ContainsKey(endpoint_map_, endpoint))
    return claimed_interfaces_[endpoint_map_[endpoint]];
  return NULL;
}

void UsbDeviceHandleImpl::PostOrSubmitTransfer(
    PlatformUsbTransferHandle transfer,
    UsbTransferType transfer_type,
    net::IOBuffer* buffer,
    size_t length,
    const UsbTransferCallback& callback) {
  if (task_runner_->RunsTasksOnCurrentThread()) {
    SubmitTransfer(transfer,
                   transfer_type,
                   buffer,
                   length,
                   base::ThreadTaskRunnerHandle::Get(),
                   callback);
  } else {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&UsbDeviceHandleImpl::SubmitTransfer,
                                      this,
                                      transfer,
                                      transfer_type,
                                      make_scoped_refptr(buffer),
                                      length,
                                      base::ThreadTaskRunnerHandle::Get(),
                                      callback));
  }
}

void UsbDeviceHandleImpl::SubmitTransfer(
    PlatformUsbTransferHandle handle,
    UsbTransferType transfer_type,
    net::IOBuffer* buffer,
    const size_t length,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const UsbTransferCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  Transfer transfer;
  transfer.transfer_type = transfer_type;
  transfer.buffer = buffer;
  transfer.length = length;
  transfer.callback = callback;
  transfer.task_runner = task_runner;

  if (!device_) {
    transfer.Complete(USB_TRANSFER_DISCONNECT, 0);
    return;
  }

  // It's OK for this method to return NULL. libusb_submit_transfer will fail if
  // it requires an interface we didn't claim.
  transfer.claimed_interface = GetClaimedInterfaceForEndpoint(handle->endpoint);

  const int rv = libusb_submit_transfer(handle);
  if (rv == LIBUSB_SUCCESS) {
    transfers_[handle] = transfer;
  } else {
    VLOG(1) << "Failed to submit transfer: "
            << ConvertPlatformUsbErrorToString(rv);
    transfer.Complete(USB_TRANSFER_ERROR, 0);
  }
}

/* static */
void LIBUSB_CALL UsbDeviceHandleImpl::PlatformTransferCallback(
    PlatformUsbTransferHandle transfer) {
  UsbDeviceHandleImpl* device_handle =
      reinterpret_cast<UsbDeviceHandleImpl*>(transfer->user_data);
  device_handle->task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &UsbDeviceHandleImpl::CompleteTransfer, device_handle, transfer));
}

void UsbDeviceHandleImpl::CompleteTransfer(PlatformUsbTransferHandle handle) {
  DCHECK(ContainsKey(transfers_, handle)) << "Missing transfer completed";

  Transfer transfer = transfers_[handle];
  transfers_.erase(handle);

  DCHECK_GE(handle->actual_length, 0) << "Negative actual length received";
  size_t actual_length =
      static_cast<size_t>(std::max(handle->actual_length, 0));

  DCHECK(transfer.length >= actual_length)
      << "data too big for our buffer (libusb failure?)";

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
                 transfer.buffer->data() + LIBUSB_CONTROL_SETUP_SIZE,
                 actual_length);
          transfer.buffer = resized_buffer;
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
              memmove(transfer.buffer->data() + actual_length,
                      transfer.buffer->data() + packet_buffer_start,
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

  transfer.Complete(ConvertTransferStatus(handle->status), actual_length);
  libusb_free_transfer(handle);

  // Must release interface first before actually delete this.
  transfer.claimed_interface = NULL;
}

bool UsbDeviceHandleImpl::GetSupportedLanguages() {
  if (!languages_.empty()) {
    return true;
  }

  // The 1-byte length field limits the descriptor to 256-bytes (128 uint16s).
  uint16 languages[128];
  int size = libusb_get_string_descriptor(
      handle_,
      0,
      0,
      reinterpret_cast<unsigned char*>(&languages[0]),
      sizeof(languages));
  if (size < 0) {
    VLOG(1) << "Failed to get list of supported languages: "
            << ConvertPlatformUsbErrorToString(size);
    return false;
  } else if (size < 2) {
    VLOG(1) << "String descriptor zero has no header.";
    return false;
    // The first 2 bytes of the descriptor are the total length and type tag.
  } else if ((languages[0] & 0xff) != size) {
    VLOG(1) << "String descriptor zero size mismatch: " << (languages[0] & 0xff)
            << " != " << size;
    return false;
  } else if ((languages[0] >> 8) != LIBUSB_DT_STRING) {
    VLOG(1) << "String descriptor zero is not a string descriptor.";
    return false;
  }

  languages_.assign(languages[1], languages[(size - 2) / 2]);
  return true;
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

}  // namespace device

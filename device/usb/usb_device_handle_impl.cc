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
#include "components/device_event_log/device_event_log.h"
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
    USB_LOG(EVENT) << "Failed to claim interface " << interface_number_ << ": "
                   << ConvertPlatformUsbErrorToString(rv);
  }
  return rv == LIBUSB_SUCCESS;
}

// This inner class owns the underlying libusb_transfer and may outlast
// the UsbDeviceHandle that created it.
class UsbDeviceHandleImpl::Transfer {
 public:
  static scoped_ptr<Transfer> CreateControlTransfer(
      uint8 type,
      uint8 request,
      uint16 value,
      uint16 index,
      uint16 length,
      scoped_refptr<net::IOBuffer> buffer,
      unsigned int timeout,
      const UsbTransferCallback& callback);
  static scoped_ptr<Transfer> CreateBulkTransfer(
      uint8 endpoint,
      scoped_refptr<net::IOBuffer> buffer,
      int length,
      unsigned int timeout,
      const UsbTransferCallback& callback);
  static scoped_ptr<Transfer> CreateInterruptTransfer(
      uint8 endpoint,
      scoped_refptr<net::IOBuffer> buffer,
      int length,
      unsigned int timeout,
      const UsbTransferCallback& callback);
  static scoped_ptr<Transfer> CreateIsochronousTransfer(
      uint8 endpoint,
      scoped_refptr<net::IOBuffer> buffer,
      size_t length,
      unsigned int packets,
      unsigned int packet_length,
      unsigned int timeout,
      const UsbTransferCallback& callback);

  ~Transfer();

  bool Submit(base::WeakPtr<UsbDeviceHandleImpl> device_handle);
  void Cancel();
  void ProcessCompletion();
  void Complete(UsbTransferStatus status, size_t bytes_transferred);

  const UsbDeviceHandleImpl::InterfaceClaimer* claimed_interface() const {
    return claimed_interface_.get();
  }

 private:
  Transfer(UsbTransferType transfer_type,
           scoped_refptr<net::IOBuffer> buffer,
           size_t length,
           const UsbTransferCallback& callback);

  static void LIBUSB_CALL PlatformCallback(PlatformUsbTransferHandle handle);

  UsbTransferType transfer_type_;
  base::WeakPtr<UsbDeviceHandleImpl> device_handle_;
  PlatformUsbTransferHandle platform_transfer_;
  scoped_refptr<net::IOBuffer> buffer_;
  scoped_refptr<UsbDeviceHandleImpl::InterfaceClaimer> claimed_interface_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  size_t length_;
  bool cancelled_;
  UsbTransferCallback callback_;
  scoped_refptr<base::SingleThreadTaskRunner> callback_task_runner_;
};

// static
scoped_ptr<UsbDeviceHandleImpl::Transfer>
UsbDeviceHandleImpl::Transfer::CreateControlTransfer(
    uint8 type,
    uint8 request,
    uint16 value,
    uint16 index,
    uint16 length,
    scoped_refptr<net::IOBuffer> buffer,
    unsigned int timeout,
    const UsbTransferCallback& callback) {
  scoped_ptr<Transfer> transfer(new Transfer(USB_TRANSFER_CONTROL, buffer,
                                             length + LIBUSB_CONTROL_SETUP_SIZE,
                                             callback));

  transfer->platform_transfer_ = libusb_alloc_transfer(0);
  if (!transfer->platform_transfer_) {
    USB_LOG(ERROR) << "Failed to allocate control transfer.";
    return nullptr;
  }

  libusb_fill_control_setup(reinterpret_cast<uint8*>(buffer->data()), type,
                            request, value, index, length);
  libusb_fill_control_transfer(transfer->platform_transfer_,
                               nullptr, /* filled in by Submit() */
                               reinterpret_cast<uint8*>(buffer->data()),
                               &UsbDeviceHandleImpl::Transfer::PlatformCallback,
                               transfer.get(), timeout);

  return transfer.Pass();
}

// static
scoped_ptr<UsbDeviceHandleImpl::Transfer>
UsbDeviceHandleImpl::Transfer::CreateBulkTransfer(
    uint8 endpoint,
    scoped_refptr<net::IOBuffer> buffer,
    int length,
    unsigned int timeout,
    const UsbTransferCallback& callback) {
  scoped_ptr<Transfer> transfer(
      new Transfer(USB_TRANSFER_BULK, buffer, length, callback));

  transfer->platform_transfer_ = libusb_alloc_transfer(0);
  if (!transfer->platform_transfer_) {
    USB_LOG(ERROR) << "Failed to allocate bulk transfer.";
    return nullptr;
  }

  libusb_fill_bulk_transfer(transfer->platform_transfer_,
                            nullptr, /* filled in by Submit() */
                            endpoint, reinterpret_cast<uint8*>(buffer->data()),
                            static_cast<int>(length),
                            &UsbDeviceHandleImpl::Transfer::PlatformCallback,
                            transfer.get(), timeout);

  return transfer.Pass();
}

// static
scoped_ptr<UsbDeviceHandleImpl::Transfer>
UsbDeviceHandleImpl::Transfer::CreateInterruptTransfer(
    uint8 endpoint,
    scoped_refptr<net::IOBuffer> buffer,
    int length,
    unsigned int timeout,
    const UsbTransferCallback& callback) {
  scoped_ptr<Transfer> transfer(
      new Transfer(USB_TRANSFER_INTERRUPT, buffer, length, callback));

  transfer->platform_transfer_ = libusb_alloc_transfer(0);
  if (!transfer->platform_transfer_) {
    USB_LOG(ERROR) << "Failed to allocate interrupt transfer.";
    return nullptr;
  }

  libusb_fill_interrupt_transfer(
      transfer->platform_transfer_, nullptr, /* filled in by Submit() */
      endpoint, reinterpret_cast<uint8*>(buffer->data()),
      static_cast<int>(length),
      &UsbDeviceHandleImpl::Transfer::PlatformCallback, transfer.get(),
      timeout);

  return transfer.Pass();
}

// static
scoped_ptr<UsbDeviceHandleImpl::Transfer>
UsbDeviceHandleImpl::Transfer::CreateIsochronousTransfer(
    uint8 endpoint,
    scoped_refptr<net::IOBuffer> buffer,
    size_t length,
    unsigned int packets,
    unsigned int packet_length,
    unsigned int timeout,
    const UsbTransferCallback& callback) {
  DCHECK(packets <= length && (packets * packet_length) <= length)
      << "transfer length is too small";

  scoped_ptr<Transfer> transfer(
      new Transfer(USB_TRANSFER_ISOCHRONOUS, buffer, length, callback));

  transfer->platform_transfer_ = libusb_alloc_transfer(packets);
  if (!transfer->platform_transfer_) {
    USB_LOG(ERROR) << "Failed to allocate isochronous transfer.";
    return nullptr;
  }

  libusb_fill_iso_transfer(
      transfer->platform_transfer_, nullptr, /* filled in by Submit() */
      endpoint, reinterpret_cast<uint8*>(buffer->data()),
      static_cast<int>(length), packets, &Transfer::PlatformCallback,
      transfer.get(), timeout);
  libusb_set_iso_packet_lengths(transfer->platform_transfer_, packet_length);

  return transfer.Pass();
}

UsbDeviceHandleImpl::Transfer::Transfer(UsbTransferType transfer_type,
                                        scoped_refptr<net::IOBuffer> buffer,
                                        size_t length,
                                        const UsbTransferCallback& callback)
    : transfer_type_(transfer_type),
      buffer_(buffer),
      length_(length),
      cancelled_(false),
      callback_(callback) {
  // Remember the thread from which this transfer was created so that |callback|
  // can be dispatched there.
  callback_task_runner_ = base::ThreadTaskRunnerHandle::Get();
}

UsbDeviceHandleImpl::Transfer::~Transfer() {
  if (platform_transfer_) {
    libusb_free_transfer(platform_transfer_);
  }
}

bool UsbDeviceHandleImpl::Transfer::Submit(
    base::WeakPtr<UsbDeviceHandleImpl> device_handle) {
  device_handle_ = device_handle;
  // Remember the thread from which this transfer was submitted so that it can
  // be marked complete there.
  task_runner_ = base::ThreadTaskRunnerHandle::Get();
  // GetClaimedInterfaceForEndpoint may return nullptr. libusb_submit_transfer
  // will fail if it requires an interface we didn't claim.
  claimed_interface_ = device_handle->GetClaimedInterfaceForEndpoint(
      platform_transfer_->endpoint);
  platform_transfer_->dev_handle = device_handle_->handle_;

  const int rv = libusb_submit_transfer(platform_transfer_);
  if (rv == LIBUSB_SUCCESS) {
    return true;
  } else {
    USB_LOG(EVENT) << "Failed to submit transfer: "
                   << ConvertPlatformUsbErrorToString(rv);
    Complete(USB_TRANSFER_ERROR, 0);
    return false;
  }
}

void UsbDeviceHandleImpl::Transfer::Cancel() {
  if (!cancelled_) {
    libusb_cancel_transfer(platform_transfer_);
    claimed_interface_ = nullptr;
  }
  cancelled_ = true;
}

void UsbDeviceHandleImpl::Transfer::ProcessCompletion() {
  DCHECK_GE(platform_transfer_->actual_length, 0)
      << "Negative actual length received";
  size_t actual_length =
      static_cast<size_t>(std::max(platform_transfer_->actual_length, 0));

  DCHECK(length_ >= actual_length)
      << "data too big for our buffer (libusb failure?)";

  switch (transfer_type_) {
    case USB_TRANSFER_CONTROL:
      // If the transfer is a control transfer we do not expose the control
      // setup header to the caller. This logic strips off the header if
      // present before invoking the callback provided with the transfer.
      if (actual_length > 0) {
        CHECK(length_ >= LIBUSB_CONTROL_SETUP_SIZE)
            << "buffer was not correctly set: too small for the control header";

        if (length_ >= (LIBUSB_CONTROL_SETUP_SIZE + actual_length)) {
          // If the payload is zero bytes long, pad out the allocated buffer
          // size to one byte so that an IOBuffer of that size can be allocated.
          scoped_refptr<net::IOBuffer> resized_buffer =
              new net::IOBuffer(static_cast<int>(
                  std::max(actual_length, static_cast<size_t>(1))));
          memcpy(resized_buffer->data(),
                 buffer_->data() + LIBUSB_CONTROL_SETUP_SIZE, actual_length);
          buffer_ = resized_buffer;
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
        for (int i = 0; i < platform_transfer_->num_iso_packets; ++i) {
          PlatformUsbIsoPacketDescriptor packet =
              &platform_transfer_->iso_packet_desc[i];
          if (packet->actual_length > 0) {
            // We don't need to copy as long as all packets until now provide
            // all the data the packet can hold.
            if (actual_length < packet_buffer_start) {
              CHECK(packet_buffer_start + packet->actual_length <= length_);
              memmove(buffer_->data() + actual_length,
                      buffer_->data() + packet_buffer_start,
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

  Complete(ConvertTransferStatus(platform_transfer_->status), actual_length);
}

void UsbDeviceHandleImpl::Transfer::Complete(UsbTransferStatus status,
                                             size_t bytes_transferred) {
  if (callback_task_runner_->RunsTasksOnCurrentThread()) {
    callback_.Run(status, buffer_, bytes_transferred);
  } else {
    callback_task_runner_->PostTask(
        FROM_HERE, base::Bind(callback_, status, buffer_, bytes_transferred));
  }
}

/* static */
void LIBUSB_CALL UsbDeviceHandleImpl::Transfer::PlatformCallback(
    PlatformUsbTransferHandle platform_transfer) {
  scoped_ptr<Transfer> transfer(
      reinterpret_cast<Transfer*>(platform_transfer->user_data));
  DCHECK(transfer->platform_transfer_ == platform_transfer);

  // Because device_handle_ is a weak pointer it is guaranteed that the callback
  // will be discarded if the handle has been freed.
  Transfer* tmp_transfer = transfer.get();  // base::Passed invalidates transfer
  tmp_transfer->task_runner_->PostTask(
      FROM_HERE, base::Bind(&UsbDeviceHandleImpl::CompleteTransfer,
                            tmp_transfer->device_handle_,
                            base::Passed(&transfer)));
}

UsbDeviceHandleImpl::UsbDeviceHandleImpl(scoped_refptr<UsbContext> context,
                                         scoped_refptr<UsbDeviceImpl> device,
                                         PlatformUsbDeviceHandle handle)
    : device_(device),
      handle_(handle),
      context_(context),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_factory_(this) {
  DCHECK(handle) << "Cannot create device with NULL handle.";
}

UsbDeviceHandleImpl::~UsbDeviceHandleImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());

  libusb_close(handle_);
  handle_ = NULL;
}

scoped_refptr<UsbDevice> UsbDeviceHandleImpl::GetDevice() const {
  return device_;
}

void UsbDeviceHandleImpl::Close() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (device_)
    device_->Close(this);
}

bool UsbDeviceHandleImpl::SetConfiguration(int configuration_value) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!device_) {
    return false;
  }

  for (Transfer* transfer : transfers_) {
    transfer->Cancel();
  }
  claimed_interfaces_.clear();

  int rv = libusb_set_configuration(handle_, configuration_value);
  if (rv == LIBUSB_SUCCESS) {
    device_->RefreshConfiguration();
    RefreshEndpointMap();
  } else {
    USB_LOG(EVENT) << "Failed to set configuration " << configuration_value
                   << ": " << ConvertPlatformUsbErrorToString(rv);
  }
  return rv == LIBUSB_SUCCESS;
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
  for (Transfer* transfer : transfers_) {
    if (transfer->claimed_interface() == interface_claimer) {
      transfer->Cancel();
    }
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
    USB_LOG(EVENT) << "Failed to set interface " << interface_number
                   << " to alternate setting " << alternate_setting << ": "
                   << ConvertPlatformUsbErrorToString(rv);
  }
  return rv == LIBUSB_SUCCESS;
}

bool UsbDeviceHandleImpl::ResetDevice() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!device_)
    return false;

  const int rv = libusb_reset_device(handle_);
  if (rv != LIBUSB_SUCCESS) {
    USB_LOG(EVENT) << "Failed to reset device: "
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
      USB_LOG(EVENT) << "Failed to get string descriptor " << string_id
                     << " (langid " << language_id
                     << "): " << ConvertPlatformUsbErrorToString(size);
      continue;
    } else if (size < 2) {
      USB_LOG(EVENT) << "String descriptor " << string_id << " (langid "
                     << language_id << ") has no header.";
      continue;
      // The first 2 bytes of the descriptor are the total length and type tag.
    } else if ((text[0] & 0xff) != size) {
      USB_LOG(EVENT) << "String descriptor " << string_id << " (langid "
                     << language_id << ") size mismatch: " << (text[0] & 0xff)
                     << " != " << size;
      continue;
    } else if ((text[0] >> 8) != LIBUSB_DT_STRING) {
      USB_LOG(EVENT) << "String descriptor " << string_id << " (langid "
                     << language_id << ") is not a string descriptor.";
      continue;
    }

    *string = base::string16(text + 1, (size - 2) / 2);
    strings_[string_id] = *string;
    return true;
  }

  return false;
}

void UsbDeviceHandleImpl::ControlTransfer(UsbEndpointDirection direction,
                                          TransferRequestType request_type,
                                          TransferRecipient recipient,
                                          uint8 request,
                                          uint16 value,
                                          uint16 index,
                                          net::IOBuffer* buffer,
                                          size_t length,
                                          unsigned int timeout,
                                          const UsbTransferCallback& callback) {
  if (length > UINT16_MAX) {
    USB_LOG(USER) << "Transfer too long.";
    callback.Run(USB_TRANSFER_ERROR, buffer, 0);
    return;
  }

  const size_t resized_length = LIBUSB_CONTROL_SETUP_SIZE + length;
  scoped_refptr<net::IOBuffer> resized_buffer(
      new net::IOBufferWithSize(static_cast<int>(resized_length)));
  if (!resized_buffer.get()) {
    callback.Run(USB_TRANSFER_ERROR, buffer, 0);
    return;
  }
  memcpy(resized_buffer->data() + LIBUSB_CONTROL_SETUP_SIZE, buffer->data(),
         length);

  scoped_ptr<Transfer> transfer = Transfer::CreateControlTransfer(
      CreateRequestType(direction, request_type, recipient), request, value,
      index, static_cast<uint16>(length), resized_buffer, timeout, callback);
  if (!transfer) {
    callback.Run(USB_TRANSFER_ERROR, buffer, 0);
    return;
  }

  PostOrSubmitTransfer(transfer.Pass());
}

void UsbDeviceHandleImpl::BulkTransfer(const UsbEndpointDirection direction,
                                       const uint8 endpoint,
                                       net::IOBuffer* buffer,
                                       const size_t length,
                                       const unsigned int timeout,
                                       const UsbTransferCallback& callback) {
  if (length > INT_MAX) {
    USB_LOG(USER) << "Transfer too long.";
    callback.Run(USB_TRANSFER_ERROR, buffer, 0);
    return;
  }

  scoped_ptr<Transfer> transfer = Transfer::CreateBulkTransfer(
      ConvertTransferDirection(direction) | endpoint, buffer,
      static_cast<int>(length), timeout, callback);

  PostOrSubmitTransfer(transfer.Pass());
}

void UsbDeviceHandleImpl::InterruptTransfer(
    UsbEndpointDirection direction,
    uint8 endpoint,
    net::IOBuffer* buffer,
    size_t length,
    unsigned int timeout,
    const UsbTransferCallback& callback) {
  if (length > INT_MAX) {
    USB_LOG(USER) << "Transfer too long.";
    callback.Run(USB_TRANSFER_ERROR, buffer, 0);
    return;
  }

  scoped_ptr<Transfer> transfer = Transfer::CreateInterruptTransfer(
      ConvertTransferDirection(direction) | endpoint, buffer,
      static_cast<int>(length), timeout, callback);

  PostOrSubmitTransfer(transfer.Pass());
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
  if (length > INT_MAX) {
    USB_LOG(USER) << "Transfer too long.";
    callback.Run(USB_TRANSFER_ERROR, buffer, 0);
    return;
  }

  scoped_ptr<Transfer> transfer = Transfer::CreateIsochronousTransfer(
      ConvertTransferDirection(direction) | endpoint, buffer,
      static_cast<int>(length), packets, packet_length, timeout, callback);

  PostOrSubmitTransfer(transfer.Pass());
}

void UsbDeviceHandleImpl::RefreshEndpointMap() {
  DCHECK(thread_checker_.CalledOnValidThread());
  endpoint_map_.clear();
  const UsbConfigDescriptor* config = device_->GetConfiguration();
  if (config) {
    for (const auto& map_entry : claimed_interfaces_) {
      int interface_number = map_entry.first;
      const scoped_refptr<InterfaceClaimer>& claimed_iface = map_entry.second;

      for (const UsbInterfaceDescriptor& iface : config->interfaces) {
        if (iface.interface_number == interface_number &&
            iface.alternate_setting == claimed_iface->alternate_setting()) {
          for (const UsbEndpointDescriptor& endpoint : iface.endpoints) {
            endpoint_map_[endpoint.address] = interface_number;
          }
          break;
        }
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

void UsbDeviceHandleImpl::PostOrSubmitTransfer(scoped_ptr<Transfer> transfer) {
  if (task_runner_->RunsTasksOnCurrentThread()) {
    SubmitTransfer(transfer.Pass());
  } else {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&UsbDeviceHandleImpl::SubmitTransfer, this,
                              base::Passed(&transfer)));
  }
}

void UsbDeviceHandleImpl::SubmitTransfer(scoped_ptr<Transfer> transfer) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (device_) {
    if (transfer->Submit(weak_factory_.GetWeakPtr())) {
      // Transfer is now owned by libusb until its completion callback is run.
      // This object holds a weak reference.
      transfers_.insert(transfer.release());
    }
  } else {
    transfer->Complete(USB_TRANSFER_DISCONNECT, 0);
  }
}

void UsbDeviceHandleImpl::CompleteTransfer(scoped_ptr<Transfer> transfer) {
  DCHECK(ContainsKey(transfers_, transfer.get()))
      << "Missing transfer completed";
  transfers_.erase(transfer.get());
  transfer->ProcessCompletion();
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
    USB_LOG(EVENT) << "Failed to get list of supported languages: "
                   << ConvertPlatformUsbErrorToString(size);
    return false;
  } else if (size < 2) {
    USB_LOG(EVENT) << "String descriptor zero has no header.";
    return false;
    // The first 2 bytes of the descriptor are the total length and type tag.
  } else if ((languages[0] & 0xff) != size) {
    USB_LOG(EVENT) << "String descriptor zero size mismatch: "
                   << (languages[0] & 0xff) << " != " << size;
    return false;
  } else if ((languages[0] >> 8) != LIBUSB_DT_STRING) {
    USB_LOG(EVENT) << "String descriptor zero is not a string descriptor.";
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
  for (Transfer* transfer : transfers_) {
    // The callback will be called some time later.
    transfer->Cancel();
  }

  // Attempt-release all the interfaces.
  // It will be retained until the transfer cancellation is finished.
  claimed_interfaces_.clear();

  // Cannot close device handle here. Need to wait for libusb_cancel_transfer to
  // finish.
  device_ = NULL;
}

}  // namespace device

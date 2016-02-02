// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_device_handle_impl.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
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
#include "net/base/io_buffer.h"
#include "third_party/libusb/src/libusb/libusb.h"

namespace device {

typedef libusb_device* PlatformUsbDevice;

void HandleTransferCompletion(PlatformUsbTransferHandle transfer);

namespace {

uint8_t ConvertTransferDirection(UsbEndpointDirection direction) {
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

uint8_t CreateRequestType(UsbEndpointDirection direction,
                          UsbDeviceHandle::TransferRequestType request_type,
                          UsbDeviceHandle::TransferRecipient recipient) {
  uint8_t result = ConvertTransferDirection(direction);

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

static void RunTransferCallback(
    scoped_refptr<base::TaskRunner> callback_task_runner,
    const UsbDeviceHandle::TransferCallback& callback,
    UsbTransferStatus status,
    scoped_refptr<net::IOBuffer> buffer,
    size_t result) {
  if (callback_task_runner->RunsTasksOnCurrentThread()) {
    callback.Run(status, buffer, result);
  } else {
    callback_task_runner->PostTask(
        FROM_HERE, base::Bind(callback, status, buffer, result));
  }
}

}  // namespace

class UsbDeviceHandleImpl::InterfaceClaimer
    : public base::RefCountedThreadSafe<UsbDeviceHandleImpl::InterfaceClaimer> {
 public:
  InterfaceClaimer(scoped_refptr<UsbDeviceHandleImpl> handle,
                   int interface_number);

  int alternate_setting() const { return alternate_setting_; }
  void set_alternate_setting(const int alternate_setting) {
    alternate_setting_ = alternate_setting;
  }

 private:
  friend class base::RefCountedThreadSafe<InterfaceClaimer>;
  ~InterfaceClaimer();

  const scoped_refptr<UsbDeviceHandleImpl> handle_;
  const int interface_number_;
  int alternate_setting_;

  DISALLOW_COPY_AND_ASSIGN(InterfaceClaimer);
};

UsbDeviceHandleImpl::InterfaceClaimer::InterfaceClaimer(
    scoped_refptr<UsbDeviceHandleImpl> handle,
    int interface_number)
    : handle_(handle),
      interface_number_(interface_number),
      alternate_setting_(0) {}

UsbDeviceHandleImpl::InterfaceClaimer::~InterfaceClaimer() {
  libusb_release_interface(handle_->handle(), interface_number_);
}

// This inner class owns the underlying libusb_transfer and may outlast
// the UsbDeviceHandle that created it.
class UsbDeviceHandleImpl::Transfer {
 public:
  static scoped_ptr<Transfer> CreateControlTransfer(
      scoped_refptr<UsbDeviceHandleImpl> device_handle,
      uint8_t type,
      uint8_t request,
      uint16_t value,
      uint16_t index,
      uint16_t length,
      scoped_refptr<net::IOBuffer> buffer,
      unsigned int timeout,
      scoped_refptr<base::TaskRunner> callback_task_runner,
      const TransferCallback& callback);
  static scoped_ptr<Transfer> CreateBulkTransfer(
      scoped_refptr<UsbDeviceHandleImpl> device_handle,
      uint8_t endpoint,
      scoped_refptr<net::IOBuffer> buffer,
      int length,
      unsigned int timeout,
      scoped_refptr<base::TaskRunner> callback_task_runner,
      const TransferCallback& callback);
  static scoped_ptr<Transfer> CreateInterruptTransfer(
      scoped_refptr<UsbDeviceHandleImpl> device_handle,
      uint8_t endpoint,
      scoped_refptr<net::IOBuffer> buffer,
      int length,
      unsigned int timeout,
      scoped_refptr<base::TaskRunner> callback_task_runner,
      const TransferCallback& callback);
  static scoped_ptr<Transfer> CreateIsochronousTransfer(
      scoped_refptr<UsbDeviceHandleImpl> device_handle,
      uint8_t endpoint,
      scoped_refptr<net::IOBuffer> buffer,
      size_t length,
      unsigned int packets,
      unsigned int packet_length,
      unsigned int timeout,
      scoped_refptr<base::TaskRunner> task_runner,
      const TransferCallback& callback);

  ~Transfer();

  void Submit();
  void Cancel();
  void ProcessCompletion();
  void TransferComplete(UsbTransferStatus status, size_t bytes_transferred);

  const UsbDeviceHandleImpl::InterfaceClaimer* claimed_interface() const {
    return claimed_interface_.get();
  }

  scoped_refptr<base::TaskRunner> callback_task_runner() const {
    return callback_task_runner_;
  }

 private:
  Transfer(scoped_refptr<UsbDeviceHandleImpl> device_handle,
           scoped_refptr<InterfaceClaimer> claimed_interface,
           UsbTransferType transfer_type,
           scoped_refptr<net::IOBuffer> buffer,
           size_t length,
           scoped_refptr<base::TaskRunner> callback_task_runner,
           const TransferCallback& callback);

  static void LIBUSB_CALL PlatformCallback(PlatformUsbTransferHandle handle);

  UsbTransferType transfer_type_;
  scoped_refptr<UsbDeviceHandleImpl> device_handle_;
  PlatformUsbTransferHandle platform_transfer_ = nullptr;
  scoped_refptr<net::IOBuffer> buffer_;
  scoped_refptr<UsbDeviceHandleImpl::InterfaceClaimer> claimed_interface_;
  size_t length_;
  bool cancelled_ = false;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_refptr<base::TaskRunner> callback_task_runner_;
  TransferCallback callback_;
};

// static
scoped_ptr<UsbDeviceHandleImpl::Transfer>
UsbDeviceHandleImpl::Transfer::CreateControlTransfer(
    scoped_refptr<UsbDeviceHandleImpl> device_handle,
    uint8_t type,
    uint8_t request,
    uint16_t value,
    uint16_t index,
    uint16_t length,
    scoped_refptr<net::IOBuffer> buffer,
    unsigned int timeout,
    scoped_refptr<base::TaskRunner> callback_task_runner,
    const TransferCallback& callback) {
  scoped_ptr<Transfer> transfer(new Transfer(
      device_handle, nullptr, USB_TRANSFER_CONTROL, buffer,
      length + LIBUSB_CONTROL_SETUP_SIZE, callback_task_runner, callback));

  transfer->platform_transfer_ = libusb_alloc_transfer(0);
  if (!transfer->platform_transfer_) {
    USB_LOG(ERROR) << "Failed to allocate control transfer.";
    return nullptr;
  }

  libusb_fill_control_setup(reinterpret_cast<uint8_t*>(buffer->data()), type,
                            request, value, index, length);
  libusb_fill_control_transfer(transfer->platform_transfer_,
                               device_handle->handle_,
                               reinterpret_cast<uint8_t*>(buffer->data()),
                               &UsbDeviceHandleImpl::Transfer::PlatformCallback,
                               transfer.get(), timeout);

  return transfer;
}

// static
scoped_ptr<UsbDeviceHandleImpl::Transfer>
UsbDeviceHandleImpl::Transfer::CreateBulkTransfer(
    scoped_refptr<UsbDeviceHandleImpl> device_handle,
    uint8_t endpoint,
    scoped_refptr<net::IOBuffer> buffer,
    int length,
    unsigned int timeout,
    scoped_refptr<base::TaskRunner> callback_task_runner,
    const TransferCallback& callback) {
  scoped_ptr<Transfer> transfer(new Transfer(
      device_handle, device_handle->GetClaimedInterfaceForEndpoint(endpoint),
      USB_TRANSFER_BULK, buffer, length, callback_task_runner, callback));

  transfer->platform_transfer_ = libusb_alloc_transfer(0);
  if (!transfer->platform_transfer_) {
    USB_LOG(ERROR) << "Failed to allocate bulk transfer.";
    return nullptr;
  }

  libusb_fill_bulk_transfer(
      transfer->platform_transfer_, device_handle->handle_, endpoint,
      reinterpret_cast<uint8_t*>(buffer->data()), static_cast<int>(length),
      &UsbDeviceHandleImpl::Transfer::PlatformCallback, transfer.get(),
      timeout);

  return transfer;
}

// static
scoped_ptr<UsbDeviceHandleImpl::Transfer>
UsbDeviceHandleImpl::Transfer::CreateInterruptTransfer(
    scoped_refptr<UsbDeviceHandleImpl> device_handle,
    uint8_t endpoint,
    scoped_refptr<net::IOBuffer> buffer,
    int length,
    unsigned int timeout,
    scoped_refptr<base::TaskRunner> callback_task_runner,
    const TransferCallback& callback) {
  scoped_ptr<Transfer> transfer(new Transfer(
      device_handle, device_handle->GetClaimedInterfaceForEndpoint(endpoint),
      USB_TRANSFER_INTERRUPT, buffer, length, callback_task_runner, callback));

  transfer->platform_transfer_ = libusb_alloc_transfer(0);
  if (!transfer->platform_transfer_) {
    USB_LOG(ERROR) << "Failed to allocate interrupt transfer.";
    return nullptr;
  }

  libusb_fill_interrupt_transfer(
      transfer->platform_transfer_, device_handle->handle_, endpoint,
      reinterpret_cast<uint8_t*>(buffer->data()), static_cast<int>(length),
      &UsbDeviceHandleImpl::Transfer::PlatformCallback, transfer.get(),
      timeout);

  return transfer;
}

// static
scoped_ptr<UsbDeviceHandleImpl::Transfer>
UsbDeviceHandleImpl::Transfer::CreateIsochronousTransfer(
    scoped_refptr<UsbDeviceHandleImpl> device_handle,
    uint8_t endpoint,
    scoped_refptr<net::IOBuffer> buffer,
    size_t length,
    unsigned int packets,
    unsigned int packet_length,
    unsigned int timeout,
    scoped_refptr<base::TaskRunner> callback_task_runner,
    const TransferCallback& callback) {
  DCHECK(packets <= length && (packets * packet_length) <= length)
      << "transfer length is too small";

  scoped_ptr<Transfer> transfer(new Transfer(
      device_handle, device_handle->GetClaimedInterfaceForEndpoint(endpoint),
      USB_TRANSFER_ISOCHRONOUS, buffer, length, callback_task_runner,
      callback));

  transfer->platform_transfer_ = libusb_alloc_transfer(packets);
  if (!transfer->platform_transfer_) {
    USB_LOG(ERROR) << "Failed to allocate isochronous transfer.";
    return nullptr;
  }

  libusb_fill_iso_transfer(
      transfer->platform_transfer_, device_handle->handle_, endpoint,
      reinterpret_cast<uint8_t*>(buffer->data()), static_cast<int>(length),
      packets, &Transfer::PlatformCallback, transfer.get(), timeout);
  libusb_set_iso_packet_lengths(transfer->platform_transfer_, packet_length);

  return transfer;
}

UsbDeviceHandleImpl::Transfer::Transfer(
    scoped_refptr<UsbDeviceHandleImpl> device_handle,
    scoped_refptr<InterfaceClaimer> claimed_interface,
    UsbTransferType transfer_type,
    scoped_refptr<net::IOBuffer> buffer,
    size_t length,
    scoped_refptr<base::TaskRunner> callback_task_runner,
    const TransferCallback& callback)
    : transfer_type_(transfer_type),
      device_handle_(device_handle),
      buffer_(buffer),
      claimed_interface_(claimed_interface),
      length_(length),
      callback_task_runner_(callback_task_runner),
      callback_(callback) {
  task_runner_ = base::ThreadTaskRunnerHandle::Get();
}

UsbDeviceHandleImpl::Transfer::~Transfer() {
  if (platform_transfer_) {
    libusb_free_transfer(platform_transfer_);
  }
}

void UsbDeviceHandleImpl::Transfer::Submit() {
  const int rv = libusb_submit_transfer(platform_transfer_);
  if (rv != LIBUSB_SUCCESS) {
    USB_LOG(EVENT) << "Failed to submit transfer: "
                   << ConvertPlatformUsbErrorToString(rv);
    TransferComplete(USB_TRANSFER_ERROR, 0);
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
          scoped_refptr<net::IOBuffer> resized_buffer = new net::IOBuffer(
              std::max(actual_length, static_cast<size_t>(1)));
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

  TransferComplete(ConvertTransferStatus(platform_transfer_->status),
                   actual_length);
}

/* static */
void LIBUSB_CALL UsbDeviceHandleImpl::Transfer::PlatformCallback(
    PlatformUsbTransferHandle platform_transfer) {
  Transfer* transfer =
      reinterpret_cast<Transfer*>(platform_transfer->user_data);
  DCHECK(transfer->platform_transfer_ == platform_transfer);
  transfer->ProcessCompletion();
}

void UsbDeviceHandleImpl::Transfer::TransferComplete(UsbTransferStatus status,
                                                     size_t bytes_transferred) {
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&UsbDeviceHandleImpl::TransferComplete, device_handle_,
                 base::Unretained(this),
                 base::Bind(callback_, status, buffer_, bytes_transferred)));
}

scoped_refptr<UsbDevice> UsbDeviceHandleImpl::GetDevice() const {
  return device_;
}

void UsbDeviceHandleImpl::Close() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (device_)
    device_->Close(this);
}

void UsbDeviceHandleImpl::SetConfiguration(int configuration_value,
                                           const ResultCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!device_) {
    callback.Run(false);
    return;
  }

  for (Transfer* transfer : transfers_) {
    transfer->Cancel();
  }
  claimed_interfaces_.clear();

  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&UsbDeviceHandleImpl::SetConfigurationOnBlockingThread, this,
                 configuration_value, callback));
}

void UsbDeviceHandleImpl::ClaimInterface(int interface_number,
                                         const ResultCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!device_) {
    callback.Run(false);
    return;
  }
  if (ContainsKey(claimed_interfaces_, interface_number)) {
    callback.Run(true);
    return;
  }

  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&UsbDeviceHandleImpl::ClaimInterfaceOnBlockingThread, this,
                 interface_number, callback));
}

bool UsbDeviceHandleImpl::ReleaseInterface(int interface_number) {
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

void UsbDeviceHandleImpl::SetInterfaceAlternateSetting(
    int interface_number,
    int alternate_setting,
    const ResultCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!device_ || !ContainsKey(claimed_interfaces_, interface_number)) {
    callback.Run(false);
    return;
  }

  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &UsbDeviceHandleImpl::SetInterfaceAlternateSettingOnBlockingThread,
          this, interface_number, alternate_setting, callback));
}

void UsbDeviceHandleImpl::ResetDevice(const ResultCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!device_) {
    callback.Run(false);
    return;
  }

  blocking_task_runner_->PostTask(
      FROM_HERE, base::Bind(&UsbDeviceHandleImpl::ResetDeviceOnBlockingThread,
                            this, callback));
}

void UsbDeviceHandleImpl::ClearHalt(uint8_t endpoint,
                                    const ResultCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!device_) {
    callback.Run(false);
    return;
  }

  InterfaceClaimer* interface_claimer =
      GetClaimedInterfaceForEndpoint(endpoint).get();
  for (Transfer* transfer : transfers_) {
    if (transfer->claimed_interface() == interface_claimer) {
      transfer->Cancel();
    }
  }

  blocking_task_runner_->PostTask(
      FROM_HERE, base::Bind(&UsbDeviceHandleImpl::ClearHaltOnBlockingThread,
                            this, endpoint, callback));
}

void UsbDeviceHandleImpl::ControlTransfer(UsbEndpointDirection direction,
                                          TransferRequestType request_type,
                                          TransferRecipient recipient,
                                          uint8_t request,
                                          uint16_t value,
                                          uint16_t index,
                                          scoped_refptr<net::IOBuffer> buffer,
                                          size_t length,
                                          unsigned int timeout,
                                          const TransferCallback& callback) {
  if (task_runner_->BelongsToCurrentThread()) {
    ControlTransferInternal(direction, request_type, recipient, request, value,
                            index, buffer, length, timeout, task_runner_,
                            callback);
  } else {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&UsbDeviceHandleImpl::ControlTransferInternal,
                              this, direction, request_type, recipient, request,
                              value, index, buffer, length, timeout,
                              base::ThreadTaskRunnerHandle::Get(), callback));
  }
}

void UsbDeviceHandleImpl::IsochronousTransfer(
    UsbEndpointDirection direction,
    uint8_t endpoint_number,
    scoped_refptr<net::IOBuffer> buffer,
    size_t length,
    unsigned int packets,
    unsigned int packet_length,
    unsigned int timeout,
    const TransferCallback& callback) {
  uint8_t endpoint_address =
      ConvertTransferDirection(direction) | endpoint_number;
  if (task_runner_->BelongsToCurrentThread()) {
    IsochronousTransferInternal(endpoint_address, buffer, length, packets,
                                packet_length, timeout, task_runner_, callback);
  } else {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&UsbDeviceHandleImpl::IsochronousTransferInternal, this,
                   endpoint_address, buffer, length, packets, packet_length,
                   timeout, base::ThreadTaskRunnerHandle::Get(), callback));
  }
}

void UsbDeviceHandleImpl::GenericTransfer(UsbEndpointDirection direction,
                                          uint8_t endpoint_number,
                                          scoped_refptr<net::IOBuffer> buffer,
                                          size_t length,
                                          unsigned int timeout,
                                          const TransferCallback& callback) {
  uint8_t endpoint_address =
      ConvertTransferDirection(direction) | endpoint_number;
  if (task_runner_->BelongsToCurrentThread()) {
    GenericTransferInternal(endpoint_address, buffer, length, timeout,
                            task_runner_, callback);
  } else {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&UsbDeviceHandleImpl::GenericTransferInternal,
                              this, endpoint_address, buffer, length, timeout,
                              base::ThreadTaskRunnerHandle::Get(), callback));
  }
}

bool UsbDeviceHandleImpl::FindInterfaceByEndpoint(uint8_t endpoint_address,
                                                  uint8_t* interface_number) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const auto endpoint_it = endpoint_map_.find(endpoint_address);
  if (endpoint_it != endpoint_map_.end()) {
    *interface_number = endpoint_it->second.interface_number;
    return true;
  }
  return false;
}

UsbDeviceHandleImpl::UsbDeviceHandleImpl(
    scoped_refptr<UsbContext> context,
    scoped_refptr<UsbDeviceImpl> device,
    PlatformUsbDeviceHandle handle,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
    : device_(device),
      handle_(handle),
      context_(context),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      blocking_task_runner_(blocking_task_runner) {
  DCHECK(handle) << "Cannot create device with NULL handle.";
}

UsbDeviceHandleImpl::~UsbDeviceHandleImpl() {
  // This class is RefCountedThreadSafe and so the destructor may be called on
  // any thread.
  libusb_close(handle_);
}

void UsbDeviceHandleImpl::SetConfigurationOnBlockingThread(
    int configuration_value,
    const ResultCallback& callback) {
  int rv = libusb_set_configuration(handle_, configuration_value);
  if (rv != LIBUSB_SUCCESS) {
    USB_LOG(EVENT) << "Failed to set configuration " << configuration_value
                   << ": " << ConvertPlatformUsbErrorToString(rv);
  }
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&UsbDeviceHandleImpl::SetConfigurationComplete,
                            this, rv == LIBUSB_SUCCESS, callback));
}

void UsbDeviceHandleImpl::SetConfigurationComplete(
    bool success,
    const ResultCallback& callback) {
  if (success) {
    device_->RefreshActiveConfiguration();
    RefreshEndpointMap();
  }
  callback.Run(success);
}

void UsbDeviceHandleImpl::ClaimInterfaceOnBlockingThread(
    int interface_number,
    const ResultCallback& callback) {
  int rv = libusb_claim_interface(handle_, interface_number);
  if (rv != LIBUSB_SUCCESS) {
    USB_LOG(EVENT) << "Failed to claim interface: "
                   << ConvertPlatformUsbErrorToString(rv);
  }
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&UsbDeviceHandleImpl::ClaimInterfaceComplete, this,
                            interface_number, rv == LIBUSB_SUCCESS, callback));
}

void UsbDeviceHandleImpl::ClaimInterfaceComplete(
    int interface_number,
    bool success,
    const ResultCallback& callback) {
  if (success) {
    claimed_interfaces_[interface_number] =
        new InterfaceClaimer(this, interface_number);
    RefreshEndpointMap();
  }
  callback.Run(success);
}

void UsbDeviceHandleImpl::SetInterfaceAlternateSettingOnBlockingThread(
    int interface_number,
    int alternate_setting,
    const ResultCallback& callback) {
  int rv = libusb_set_interface_alt_setting(handle_, interface_number,
                                            alternate_setting);
  if (rv != LIBUSB_SUCCESS) {
    USB_LOG(EVENT) << "Failed to set interface " << interface_number
                   << " to alternate setting " << alternate_setting << ": "
                   << ConvertPlatformUsbErrorToString(rv);
  }
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&UsbDeviceHandleImpl::SetInterfaceAlternateSettingComplete,
                 this, interface_number, alternate_setting,
                 rv == LIBUSB_SUCCESS, callback));
}

void UsbDeviceHandleImpl::SetInterfaceAlternateSettingComplete(
    int interface_number,
    int alternate_setting,
    bool success,
    const ResultCallback& callback) {
  if (success) {
    claimed_interfaces_[interface_number]->set_alternate_setting(
        alternate_setting);
    RefreshEndpointMap();
  }
  callback.Run(success);
}

void UsbDeviceHandleImpl::ResetDeviceOnBlockingThread(
    const ResultCallback& callback) {
  int rv = libusb_reset_device(handle_);
  if (rv != LIBUSB_SUCCESS) {
    USB_LOG(EVENT) << "Failed to reset device: "
                   << ConvertPlatformUsbErrorToString(rv);
  }
  task_runner_->PostTask(FROM_HERE, base::Bind(callback, rv == LIBUSB_SUCCESS));
}

void UsbDeviceHandleImpl::ClearHaltOnBlockingThread(
    uint8_t endpoint,
    const ResultCallback& callback) {
  int rv = libusb_clear_halt(handle_, endpoint);
  if (rv != LIBUSB_SUCCESS) {
    USB_LOG(EVENT) << "Failed to clear halt: "
                   << ConvertPlatformUsbErrorToString(rv);
  }
  task_runner_->PostTask(FROM_HERE, base::Bind(callback, rv == LIBUSB_SUCCESS));
}

void UsbDeviceHandleImpl::RefreshEndpointMap() {
  DCHECK(thread_checker_.CalledOnValidThread());
  endpoint_map_.clear();
  const UsbConfigDescriptor* config = device_->GetActiveConfiguration();
  if (config) {
    for (const auto& map_entry : claimed_interfaces_) {
      int interface_number = map_entry.first;
      const scoped_refptr<InterfaceClaimer>& claimed_iface = map_entry.second;

      for (const UsbInterfaceDescriptor& iface : config->interfaces) {
        if (iface.interface_number == interface_number &&
            iface.alternate_setting == claimed_iface->alternate_setting()) {
          for (const UsbEndpointDescriptor& endpoint : iface.endpoints) {
            endpoint_map_[endpoint.address] = {interface_number,
                                               endpoint.transfer_type};
          }
          break;
        }
      }
    }
  }
}

scoped_refptr<UsbDeviceHandleImpl::InterfaceClaimer>
UsbDeviceHandleImpl::GetClaimedInterfaceForEndpoint(uint8_t endpoint) {
  const auto endpoint_it = endpoint_map_.find(endpoint);
  if (endpoint_it != endpoint_map_.end())
    return claimed_interfaces_[endpoint_it->second.interface_number];
  return nullptr;
}

void UsbDeviceHandleImpl::ControlTransferInternal(
    UsbEndpointDirection direction,
    TransferRequestType request_type,
    TransferRecipient recipient,
    uint8_t request,
    uint16_t value,
    uint16_t index,
    scoped_refptr<net::IOBuffer> buffer,
    size_t length,
    unsigned int timeout,
    scoped_refptr<base::TaskRunner> callback_task_runner,
    const TransferCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!device_) {
    RunTransferCallback(callback_task_runner, callback, USB_TRANSFER_DISCONNECT,
                        buffer, 0);
    return;
  }

  if (!base::IsValueInRangeForNumericType<uint16_t>(length)) {
    USB_LOG(USER) << "Transfer too long.";
    RunTransferCallback(callback_task_runner, callback, USB_TRANSFER_ERROR,
                        buffer, 0);
    return;
  }

  const size_t resized_length = LIBUSB_CONTROL_SETUP_SIZE + length;
  scoped_refptr<net::IOBuffer> resized_buffer =
      new net::IOBufferWithSize(resized_length);
  if (!resized_buffer.get()) {
    RunTransferCallback(callback_task_runner, callback, USB_TRANSFER_ERROR,
                        buffer, 0);
    return;
  }
  memcpy(resized_buffer->data() + LIBUSB_CONTROL_SETUP_SIZE, buffer->data(),
         length);

  scoped_ptr<Transfer> transfer = Transfer::CreateControlTransfer(
      this, CreateRequestType(direction, request_type, recipient), request,
      value, index, static_cast<uint16_t>(length), resized_buffer, timeout,
      callback_task_runner, callback);
  if (!transfer) {
    RunTransferCallback(callback_task_runner, callback, USB_TRANSFER_ERROR,
                        buffer, 0);
    return;
  }

  SubmitTransfer(std::move(transfer));
}

void UsbDeviceHandleImpl::IsochronousTransferInternal(
    uint8_t endpoint_address,
    scoped_refptr<net::IOBuffer> buffer,
    size_t length,
    unsigned int packets,
    unsigned int packet_length,
    unsigned int timeout,
    scoped_refptr<base::TaskRunner> callback_task_runner,
    const TransferCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!device_) {
    RunTransferCallback(callback_task_runner, callback, USB_TRANSFER_DISCONNECT,
                        buffer, 0);
    return;
  }

  if (!base::IsValueInRangeForNumericType<int>(length)) {
    USB_LOG(USER) << "Transfer too long.";
    RunTransferCallback(callback_task_runner, callback, USB_TRANSFER_ERROR,
                        buffer, 0);
    return;
  }

  scoped_ptr<Transfer> transfer = Transfer::CreateIsochronousTransfer(
      this, endpoint_address, buffer, static_cast<int>(length), packets,
      packet_length, timeout, callback_task_runner, callback);

  SubmitTransfer(std::move(transfer));
}

void UsbDeviceHandleImpl::GenericTransferInternal(
    uint8_t endpoint_address,
    scoped_refptr<net::IOBuffer> buffer,
    size_t length,
    unsigned int timeout,
    scoped_refptr<base::TaskRunner> callback_task_runner,
    const TransferCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!device_) {
    RunTransferCallback(callback_task_runner, callback, USB_TRANSFER_DISCONNECT,
                        buffer, 0);
    return;
  }

  const auto endpoint_it = endpoint_map_.find(endpoint_address);
  if (endpoint_it == endpoint_map_.end()) {
    USB_LOG(DEBUG) << "Failed to submit transfer because endpoint "
                   << static_cast<int>(endpoint_address)
                   << " not part of a claimed interface.";
    RunTransferCallback(callback_task_runner, callback, USB_TRANSFER_ERROR,
                        buffer, 0);
    return;
  }

  if (!base::IsValueInRangeForNumericType<int>(length)) {
    USB_LOG(DEBUG) << "Transfer too long.";
    RunTransferCallback(callback_task_runner, callback, USB_TRANSFER_ERROR,
                        buffer, 0);
    return;
  }

  scoped_ptr<Transfer> transfer;
  UsbTransferType transfer_type = endpoint_it->second.transfer_type;
  if (transfer_type == USB_TRANSFER_BULK) {
    transfer = Transfer::CreateBulkTransfer(this, endpoint_address, buffer,
                                            static_cast<int>(length), timeout,
                                            callback_task_runner, callback);
  } else if (transfer_type == USB_TRANSFER_INTERRUPT) {
    transfer = Transfer::CreateInterruptTransfer(
        this, endpoint_address, buffer, static_cast<int>(length), timeout,
        callback_task_runner, callback);
  } else {
    USB_LOG(DEBUG) << "Endpoint " << static_cast<int>(endpoint_address)
                   << " is not a bulk or interrupt endpoint.";
    RunTransferCallback(callback_task_runner, callback, USB_TRANSFER_ERROR,
                        buffer, 0);
    return;
  }

  SubmitTransfer(std::move(transfer));
}

void UsbDeviceHandleImpl::SubmitTransfer(scoped_ptr<Transfer> transfer) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Transfer is owned by libusb until its completion callback is run. This
  // object holds a weak reference.
  transfers_.insert(transfer.get());
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&Transfer::Submit, base::Unretained(transfer.release())));
}

void UsbDeviceHandleImpl::TransferComplete(Transfer* transfer,
                                           const base::Closure& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(ContainsKey(transfers_, transfer)) << "Missing transfer completed";
  transfers_.erase(transfer);

  if (transfer->callback_task_runner()->RunsTasksOnCurrentThread()) {
    callback.Run();
  } else {
    transfer->callback_task_runner()->PostTask(FROM_HERE, callback);
  }

  // libusb_free_transfer races with libusb_submit_transfer and only work-
  // around is to make sure to call them on the same thread.
  blocking_task_runner_->DeleteSoon(FROM_HERE, transfer);
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

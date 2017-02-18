// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_device_handle_win.h"

#include <usbioctl.h>
#include <usbspec.h>
#include <winioctl.h>
#include <winusb.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/win/object_watcher.h"
#include "components/device_event_log/device_event_log.h"
#include "device/usb/usb_context.h"
#include "device/usb/usb_descriptors.h"
#include "device/usb/usb_device_win.h"
#include "device/usb/usb_service.h"
#include "net/base/io_buffer.h"

namespace device {

// Encapsulates waiting for the completion of an overlapped event.
class UsbDeviceHandleWin::Request : public base::win::ObjectWatcher::Delegate {
 public:
  explicit Request(HANDLE handle)
      : handle_(handle), event_(CreateEvent(nullptr, false, false, nullptr)) {
    memset(&overlapped_, 0, sizeof(overlapped_));
    overlapped_.hEvent = event_.Get();
  }

  ~Request() override {
    if (callback_) {
      SetLastError(ERROR_REQUEST_ABORTED);
      std::move(callback_).Run(this, false, 0);
    }
  }

  // Starts watching for completion of the overlapped event.
  void MaybeStartWatching(
      BOOL success,
      base::OnceCallback<void(Request*, DWORD, size_t)> callback) {
    callback_ = std::move(callback);
    if (success) {
      OnObjectSignaled(event_.Get());
    } else {
      DWORD error = GetLastError();
      if (error == ERROR_IO_PENDING) {
        watcher_.StartWatchingOnce(event_.Get(), this);
      } else {
        std::move(callback_).Run(this, error, 0);
      }
    }
  }

  OVERLAPPED* overlapped() { return &overlapped_; }

  // base::win::ObjectWatcher::Delegate
  void OnObjectSignaled(HANDLE object) override {
    DCHECK_EQ(object, event_.Get());
    DWORD size;
    if (GetOverlappedResult(handle_, &overlapped_, &size, true))
      std::move(callback_).Run(this, ERROR_SUCCESS, size);
    else
      std::move(callback_).Run(this, GetLastError(), 0);
  }

 private:
  HANDLE handle_;
  OVERLAPPED overlapped_;
  base::win::ScopedHandle event_;
  base::win::ObjectWatcher watcher_;
  base::OnceCallback<void(Request*, DWORD, size_t)> callback_;

  DISALLOW_COPY_AND_ASSIGN(Request);
};

scoped_refptr<UsbDevice> UsbDeviceHandleWin::GetDevice() const {
  return device_;
}

void UsbDeviceHandleWin::Close() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!device_)
    return;

  device_->HandleClosed(this);
  device_ = nullptr;

  if (hub_handle_.IsValid()) {
    CancelIo(hub_handle_.Get());
    hub_handle_.Close();
  }

  requests_.clear();
}

void UsbDeviceHandleWin::SetConfiguration(int configuration_value,
                                          const ResultCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!device_) {
    callback.Run(false);
    return;
  }
}

void UsbDeviceHandleWin::ClaimInterface(int interface_number,
                                        const ResultCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!device_) {
    callback.Run(false);
    return;
  }
}

void UsbDeviceHandleWin::ReleaseInterface(int interface_number,
                                          const ResultCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!device_) {
    task_runner_->PostTask(FROM_HERE, base::Bind(callback, false));
    return;
  }
}

void UsbDeviceHandleWin::SetInterfaceAlternateSetting(
    int interface_number,
    int alternate_setting,
    const ResultCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!device_) {
    callback.Run(false);
    return;
  }
}

void UsbDeviceHandleWin::ResetDevice(const ResultCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!device_) {
    callback.Run(false);
    return;
  }
}

void UsbDeviceHandleWin::ClearHalt(uint8_t endpoint,
                                   const ResultCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!device_) {
    callback.Run(false);
    return;
  }
}

void UsbDeviceHandleWin::ControlTransfer(UsbEndpointDirection direction,
                                         TransferRequestType request_type,
                                         TransferRecipient recipient,
                                         uint8_t request,
                                         uint16_t value,
                                         uint16_t index,
                                         scoped_refptr<net::IOBuffer> buffer,
                                         size_t length,
                                         unsigned int timeout,
                                         const TransferCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!device_) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(callback, USB_TRANSFER_DISCONNECT, nullptr, 0));
    return;
  }

  if (hub_handle_.IsValid()) {
    if (direction == USB_DIRECTION_INBOUND &&
        request_type == TransferRequestType::STANDARD &&
        recipient == TransferRecipient::DEVICE &&
        request == USB_REQUEST_GET_DESCRIPTOR) {
      if ((value >> 8) == USB_DEVICE_DESCRIPTOR_TYPE) {
        auto* node_connection_info = new USB_NODE_CONNECTION_INFORMATION_EX;
        node_connection_info->ConnectionIndex = device_->port_number();

        Request* request = MakeRequest(hub_handle_.Get());
        request->MaybeStartWatching(
            DeviceIoControl(hub_handle_.Get(),
                            IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX,
                            node_connection_info, sizeof(*node_connection_info),
                            node_connection_info, sizeof(*node_connection_info),
                            nullptr, request->overlapped()),
            base::Bind(&UsbDeviceHandleWin::GotNodeConnectionInformation,
                       weak_factory_.GetWeakPtr(), callback,
                       base::Owned(node_connection_info), buffer, length));
        return;
      } else if (((value >> 8) == USB_CONFIGURATION_DESCRIPTOR_TYPE) ||
                 ((value >> 8) == USB_STRING_DESCRIPTOR_TYPE)) {
        size_t size = sizeof(USB_DESCRIPTOR_REQUEST) + length;
        scoped_refptr<net::IOBuffer> request_buffer(new net::IOBuffer(size));
        USB_DESCRIPTOR_REQUEST* descriptor_request =
            reinterpret_cast<USB_DESCRIPTOR_REQUEST*>(request_buffer->data());
        descriptor_request->ConnectionIndex = device_->port_number();
        descriptor_request->SetupPacket.bmRequest = BMREQUEST_DEVICE_TO_HOST;
        descriptor_request->SetupPacket.bRequest = USB_REQUEST_GET_DESCRIPTOR;
        descriptor_request->SetupPacket.wValue = value;
        descriptor_request->SetupPacket.wIndex = index;
        descriptor_request->SetupPacket.wLength = length;

        Request* request = MakeRequest(hub_handle_.Get());
        request->MaybeStartWatching(
            DeviceIoControl(hub_handle_.Get(),
                            IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION,
                            request_buffer->data(), size,
                            request_buffer->data(), size, nullptr,
                            request->overlapped()),
            base::Bind(&UsbDeviceHandleWin::GotDescriptorFromNodeConnection,
                       weak_factory_.GetWeakPtr(), callback, request_buffer,
                       buffer, length));
        return;
      }
    }

    // Unsupported transfer for hub.
    task_runner_->PostTask(
        FROM_HERE, base::Bind(callback, USB_TRANSFER_ERROR, nullptr, 0));
    return;
  }

  // Regular control transfers unimplemented.
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(callback, USB_TRANSFER_ERROR, nullptr, 0));
}

void UsbDeviceHandleWin::IsochronousTransferIn(
    uint8_t endpoint_number,
    const std::vector<uint32_t>& packet_lengths,
    unsigned int timeout,
    const IsochronousTransferCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void UsbDeviceHandleWin::IsochronousTransferOut(
    uint8_t endpoint_number,
    scoped_refptr<net::IOBuffer> buffer,
    const std::vector<uint32_t>& packet_lengths,
    unsigned int timeout,
    const IsochronousTransferCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void UsbDeviceHandleWin::GenericTransfer(UsbEndpointDirection direction,
                                         uint8_t endpoint_number,
                                         scoped_refptr<net::IOBuffer> buffer,
                                         size_t length,
                                         unsigned int timeout,
                                         const TransferCallback& callback) {
  // This one must be callable from any thread.
}

const UsbInterfaceDescriptor* UsbDeviceHandleWin::FindInterfaceByEndpoint(
    uint8_t endpoint_address) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return nullptr;
}

UsbDeviceHandleWin::UsbDeviceHandleWin(
    scoped_refptr<UsbDeviceWin> device,
    base::win::ScopedHandle handle,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
    : device_(std::move(device)),
      hub_handle_(std::move(handle)),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      blocking_task_runner_(std::move(blocking_task_runner)),
      weak_factory_(this) {}

UsbDeviceHandleWin::~UsbDeviceHandleWin() {}

UsbDeviceHandleWin::Request* UsbDeviceHandleWin::MakeRequest(HANDLE handle) {
  auto request = base::MakeUnique<Request>(hub_handle_.Get());
  Request* request_ptr = request.get();
  requests_[request_ptr] = std::move(request);
  return request_ptr;
}

std::unique_ptr<UsbDeviceHandleWin::Request> UsbDeviceHandleWin::UnlinkRequest(
    UsbDeviceHandleWin::Request* request_ptr) {
  auto it = requests_.find(request_ptr);
  DCHECK(it != requests_.end());
  std::unique_ptr<Request> request = std::move(it->second);
  requests_.erase(it);
  return request;
}

void UsbDeviceHandleWin::GotNodeConnectionInformation(
    const TransferCallback& callback,
    void* node_connection_info_ptr,
    scoped_refptr<net::IOBuffer> buffer,
    size_t buffer_length,
    Request* request_ptr,
    DWORD win32_result,
    size_t bytes_transferred) {
  USB_NODE_CONNECTION_INFORMATION_EX* node_connection_info =
      static_cast<USB_NODE_CONNECTION_INFORMATION_EX*>(
          node_connection_info_ptr);
  std::unique_ptr<Request> request = UnlinkRequest(request_ptr);

  if (win32_result != ERROR_SUCCESS) {
    SetLastError(win32_result);
    USB_PLOG(ERROR) << "Failed to get node connection information";
    callback.Run(USB_TRANSFER_ERROR, nullptr, 0);
    return;
  }

  DCHECK_EQ(bytes_transferred, sizeof(USB_NODE_CONNECTION_INFORMATION_EX));
  bytes_transferred = std::min(sizeof(USB_DEVICE_DESCRIPTOR), buffer_length);
  memcpy(buffer->data(), &node_connection_info->DeviceDescriptor,
         bytes_transferred);
  callback.Run(USB_TRANSFER_COMPLETED, buffer, bytes_transferred);
}

void UsbDeviceHandleWin::GotDescriptorFromNodeConnection(
    const TransferCallback& callback,
    scoped_refptr<net::IOBuffer> request_buffer,
    scoped_refptr<net::IOBuffer> original_buffer,
    size_t original_buffer_length,
    Request* request_ptr,
    DWORD win32_result,
    size_t bytes_transferred) {
  std::unique_ptr<Request> request = UnlinkRequest(request_ptr);

  if (win32_result != ERROR_SUCCESS) {
    SetLastError(win32_result);
    USB_PLOG(ERROR) << "Failed to read descriptor from node connection";
    callback.Run(USB_TRANSFER_ERROR, nullptr, 0);
    return;
  }

  DCHECK_GE(bytes_transferred, sizeof(USB_DESCRIPTOR_REQUEST));
  bytes_transferred -= sizeof(USB_DESCRIPTOR_REQUEST);
  memcpy(original_buffer->data(),
         request_buffer->data() + sizeof(USB_DESCRIPTOR_REQUEST),
         bytes_transferred);
  callback.Run(USB_TRANSFER_COMPLETED, original_buffer, bytes_transferred);
}

}  // namespace device

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_device_impl.h"

#include <stddef.h>

#include <algorithm>

#include "base/bind.h"
#include "base/location.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/device_event_log/device_event_log.h"
#include "device/usb/usb_context.h"
#include "device/usb/usb_descriptors.h"
#include "device/usb/usb_device_handle_impl.h"
#include "device/usb/usb_error.h"
#include "third_party/libusb/src/libusb/libusb.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/permission_broker_client.h"
#include "dbus/file_descriptor.h"  // nogncheck
#endif  // defined(OS_CHROMEOS)

namespace device {

namespace {

UsbEndpointDirection GetDirection(
    const libusb_endpoint_descriptor* descriptor) {
  switch (descriptor->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) {
    case LIBUSB_ENDPOINT_IN:
      return USB_DIRECTION_INBOUND;
    case LIBUSB_ENDPOINT_OUT:
      return USB_DIRECTION_OUTBOUND;
    default:
      NOTREACHED();
      return USB_DIRECTION_INBOUND;
  }
}

UsbSynchronizationType GetSynchronizationType(
    const libusb_endpoint_descriptor* descriptor) {
  switch ((descriptor->bmAttributes & LIBUSB_ISO_SYNC_TYPE_MASK) >> 2) {
    case LIBUSB_ISO_SYNC_TYPE_NONE:
      return USB_SYNCHRONIZATION_NONE;
    case LIBUSB_ISO_SYNC_TYPE_ASYNC:
      return USB_SYNCHRONIZATION_ASYNCHRONOUS;
    case LIBUSB_ISO_SYNC_TYPE_ADAPTIVE:
      return USB_SYNCHRONIZATION_ADAPTIVE;
    case LIBUSB_ISO_SYNC_TYPE_SYNC:
      return USB_SYNCHRONIZATION_SYNCHRONOUS;
    default:
      NOTREACHED();
      return USB_SYNCHRONIZATION_NONE;
  }
}

UsbTransferType GetTransferType(const libusb_endpoint_descriptor* descriptor) {
  switch (descriptor->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) {
    case LIBUSB_TRANSFER_TYPE_CONTROL:
      return USB_TRANSFER_CONTROL;
    case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS:
      return USB_TRANSFER_ISOCHRONOUS;
    case LIBUSB_TRANSFER_TYPE_BULK:
      return USB_TRANSFER_BULK;
    case LIBUSB_TRANSFER_TYPE_INTERRUPT:
      return USB_TRANSFER_INTERRUPT;
    default:
      NOTREACHED();
      return USB_TRANSFER_CONTROL;
  }
}

UsbUsageType GetUsageType(const libusb_endpoint_descriptor* descriptor) {
  switch ((descriptor->bmAttributes & LIBUSB_ISO_USAGE_TYPE_MASK) >> 4) {
    case LIBUSB_ISO_USAGE_TYPE_DATA:
      return USB_USAGE_DATA;
    case LIBUSB_ISO_USAGE_TYPE_FEEDBACK:
      return USB_USAGE_FEEDBACK;
    case LIBUSB_ISO_USAGE_TYPE_IMPLICIT:
      return USB_USAGE_EXPLICIT_FEEDBACK;
    default:
      NOTREACHED();
      return USB_USAGE_DATA;
  }
}

void ConvertConfigDescriptor(const libusb_config_descriptor* platform_config,
                             UsbConfigDescriptor* configuration) {
  for (size_t i = 0; i < platform_config->bNumInterfaces; ++i) {
    const struct libusb_interface* platform_interface =
        &platform_config->interface[i];
    for (int j = 0; j < platform_interface->num_altsetting; ++j) {
      const struct libusb_interface_descriptor* platform_alt_setting =
          &platform_interface->altsetting[j];
      UsbInterfaceDescriptor interface(
          platform_alt_setting->bInterfaceNumber,
          platform_alt_setting->bAlternateSetting,
          platform_alt_setting->bInterfaceClass,
          platform_alt_setting->bInterfaceSubClass,
          platform_alt_setting->bInterfaceProtocol);

      interface.endpoints.reserve(platform_alt_setting->bNumEndpoints);
      for (size_t k = 0; k < platform_alt_setting->bNumEndpoints; ++k) {
        const struct libusb_endpoint_descriptor* platform_endpoint =
            &platform_alt_setting->endpoint[k];
        UsbEndpointDescriptor endpoint(
            platform_endpoint->bEndpointAddress,
            GetDirection(platform_endpoint), platform_endpoint->wMaxPacketSize,
            GetSynchronizationType(platform_endpoint),
            GetTransferType(platform_endpoint), GetUsageType(platform_endpoint),
            platform_endpoint->bInterval);
        endpoint.extra_data.assign(
            platform_endpoint->extra,
            platform_endpoint->extra + platform_endpoint->extra_length);

        interface.endpoints.push_back(endpoint);
      }

      interface.extra_data.assign(
          platform_alt_setting->extra,
          platform_alt_setting->extra + platform_alt_setting->extra_length);

      configuration->interfaces.push_back(interface);
    }
  }

  configuration->extra_data.assign(
      platform_config->extra,
      platform_config->extra + platform_config->extra_length);
}

}  // namespace

UsbDeviceImpl::UsbDeviceImpl(
    scoped_refptr<UsbContext> context,
    PlatformUsbDevice platform_device,
    uint16_t vendor_id,
    uint16_t product_id,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
    : UsbDevice(vendor_id,
                product_id,
                base::string16(),
                base::string16(),
                base::string16()),
      platform_device_(platform_device),
      context_(context),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      blocking_task_runner_(blocking_task_runner) {
  CHECK(platform_device) << "platform_device cannot be NULL";
  libusb_ref_device(platform_device);
  ReadAllConfigurations();
  RefreshActiveConfiguration();
}

UsbDeviceImpl::~UsbDeviceImpl() {
  // The destructor must be safe to call from any thread.
  libusb_unref_device(platform_device_);
}

#if defined(OS_CHROMEOS)

void UsbDeviceImpl::CheckUsbAccess(const ResultCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  chromeos::PermissionBrokerClient* client =
      chromeos::DBusThreadManager::Get()->GetPermissionBrokerClient();
  DCHECK(client) << "Could not get permission broker client.";
  client->CheckPathAccess(device_path_, callback);
}

#endif  // defined(OS_CHROMEOS)

void UsbDeviceImpl::Open(const OpenCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

#if defined(OS_CHROMEOS)
  chromeos::PermissionBrokerClient* client =
      chromeos::DBusThreadManager::Get()->GetPermissionBrokerClient();
  DCHECK(client) << "Could not get permission broker client.";
  client->OpenPath(
      device_path_,
      base::Bind(&UsbDeviceImpl::OnOpenRequestComplete, this, callback),
      base::Bind(&UsbDeviceImpl::OnOpenRequestError, this, callback));
#else
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&UsbDeviceImpl::OpenOnBlockingThread, this, callback));
#endif  // defined(OS_CHROMEOS)
}

void UsbDeviceImpl::Close(scoped_refptr<UsbDeviceHandle> handle) {
  DCHECK(thread_checker_.CalledOnValidThread());

  for (HandlesVector::iterator it = handles_.begin(); it != handles_.end();
       ++it) {
    if (it->get() == handle.get()) {
      (*it)->InternalClose();
      handles_.erase(it);
      return;
    }
  }
}

const UsbConfigDescriptor* UsbDeviceImpl::GetActiveConfiguration() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return active_configuration_;
}

void UsbDeviceImpl::OnDisconnect() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Swap the list of handles into a local variable because closing all open
  // handles may release the last reference to this object.
  HandlesVector handles;
  swap(handles, handles_);

  for (const scoped_refptr<UsbDeviceHandleImpl>& handle : handles_) {
    handle->InternalClose();
  }
}

void UsbDeviceImpl::ReadAllConfigurations() {
  libusb_device_descriptor device_descriptor;
  int rv = libusb_get_device_descriptor(platform_device_, &device_descriptor);
  if (rv == LIBUSB_SUCCESS) {
    uint8_t num_configurations = device_descriptor.bNumConfigurations;
    configurations_.reserve(num_configurations);
    for (uint8_t i = 0; i < num_configurations; ++i) {
      libusb_config_descriptor* platform_config;
      rv = libusb_get_config_descriptor(platform_device_, i, &platform_config);
      if (rv != LIBUSB_SUCCESS) {
        USB_LOG(EVENT) << "Failed to get config descriptor: "
                       << ConvertPlatformUsbErrorToString(rv);
        continue;
      }

      UsbConfigDescriptor config_descriptor(
          platform_config->bConfigurationValue,
          (platform_config->bmAttributes & 0x40) != 0,
          (platform_config->bmAttributes & 0x20) != 0,
          platform_config->MaxPower * 2);
      ConvertConfigDescriptor(platform_config, &config_descriptor);
      configurations_.push_back(config_descriptor);
      libusb_free_config_descriptor(platform_config);
    }
  } else {
    USB_LOG(EVENT) << "Failed to get device descriptor: "
                   << ConvertPlatformUsbErrorToString(rv);
  }
}

void UsbDeviceImpl::RefreshActiveConfiguration() {
  active_configuration_ = nullptr;
  libusb_config_descriptor* platform_config;
  int rv =
      libusb_get_active_config_descriptor(platform_device_, &platform_config);
  if (rv != LIBUSB_SUCCESS) {
    USB_LOG(EVENT) << "Failed to get config descriptor: "
                   << ConvertPlatformUsbErrorToString(rv);
    return;
  }

  for (const auto& config : configurations_) {
    if (config.configuration_value == platform_config->bConfigurationValue) {
      active_configuration_ = &config;
      break;
    }
  }

  libusb_free_config_descriptor(platform_config);
}

#if defined(OS_CHROMEOS)

void UsbDeviceImpl::OnOpenRequestComplete(const OpenCallback& callback,
                                          dbus::FileDescriptor fd) {
  blocking_task_runner_->PostTask(
      FROM_HERE, base::Bind(&UsbDeviceImpl::OpenOnBlockingThreadWithFd, this,
                            base::Passed(&fd), callback));
}

void UsbDeviceImpl::OnOpenRequestError(const OpenCallback& callback,
                                       const std::string& error_name,
                                       const std::string& error_message) {
  USB_LOG(EVENT) << "Permission broker failed to open the device: "
                 << error_name << ": " << error_message;
  callback.Run(nullptr);
}

void UsbDeviceImpl::OpenOnBlockingThreadWithFd(dbus::FileDescriptor fd,
                                               const OpenCallback& callback) {
  fd.CheckValidity();
  DCHECK(fd.is_valid());

  PlatformUsbDeviceHandle handle;
  const int rv = libusb_open_fd(platform_device_, fd.TakeValue(), &handle);
  if (LIBUSB_SUCCESS == rv) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&UsbDeviceImpl::Opened, this, handle, callback));
  } else {
    USB_LOG(EVENT) << "Failed to open device: "
                   << ConvertPlatformUsbErrorToString(rv);
    task_runner_->PostTask(FROM_HERE, base::Bind(callback, nullptr));
  }
}

#endif  // defined(OS_CHROMEOS)

void UsbDeviceImpl::OpenOnBlockingThread(const OpenCallback& callback) {
  PlatformUsbDeviceHandle handle;
  const int rv = libusb_open(platform_device_, &handle);
  if (LIBUSB_SUCCESS == rv) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&UsbDeviceImpl::Opened, this, handle, callback));
  } else {
    USB_LOG(EVENT) << "Failed to open device: "
                   << ConvertPlatformUsbErrorToString(rv);
    task_runner_->PostTask(FROM_HERE, base::Bind(callback, nullptr));
  }
}

void UsbDeviceImpl::Opened(PlatformUsbDeviceHandle platform_handle,
                           const OpenCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  scoped_refptr<UsbDeviceHandleImpl> device_handle = new UsbDeviceHandleImpl(
      context_, this, platform_handle, blocking_task_runner_);
  handles_.push_back(device_handle);
  callback.Run(device_handle);
}

}  // namespace device

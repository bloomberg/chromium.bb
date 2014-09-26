// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_device_impl.h"

#include <algorithm>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "device/usb/usb_context.h"
#include "device/usb/usb_descriptors.h"
#include "device/usb/usb_device_handle_impl.h"
#include "device/usb/usb_error.h"
#include "third_party/libusb/src/libusb/libusb.h"

#if defined(OS_CHROMEOS)
#include "base/sys_info.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/permission_broker_client.h"
#endif  // defined(OS_CHROMEOS)

#if defined(USE_UDEV)
#include "device/udev_linux/udev.h"
#endif  // defined(USE_UDEV)

namespace device {

namespace {

#if defined(OS_CHROMEOS)
void OnRequestUsbAccessReplied(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::Callback<void(bool success)>& callback,
    bool success) {
  task_runner->PostTask(FROM_HERE, base::Bind(callback, success));
}
#endif  // defined(OS_CHROMEOS)

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
  switch (descriptor->bmAttributes & LIBUSB_ISO_SYNC_TYPE_MASK) {
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
  switch (descriptor->bmAttributes & LIBUSB_ISO_USAGE_TYPE_MASK) {
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

}  // namespace

UsbDevice::UsbDevice(uint16 vendor_id, uint16 product_id, uint32 unique_id)
    : vendor_id_(vendor_id), product_id_(product_id), unique_id_(unique_id) {
}

UsbDevice::~UsbDevice() {
}

void UsbDevice::NotifyDisconnect() {
  FOR_EACH_OBSERVER(Observer, observer_list_, OnDisconnect(this));
}

UsbDeviceImpl::UsbDeviceImpl(
    scoped_refptr<UsbContext> context,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    PlatformUsbDevice platform_device,
    uint16 vendor_id,
    uint16 product_id,
    uint32 unique_id)
    : UsbDevice(vendor_id, product_id, unique_id),
      platform_device_(platform_device),
      current_configuration_cached_(false),
      context_(context),
      ui_task_runner_(ui_task_runner) {
  CHECK(platform_device) << "platform_device cannot be NULL";
  libusb_ref_device(platform_device);

#if defined(USE_UDEV)
  ScopedUdevPtr udev(udev_new());
  ScopedUdevEnumeratePtr enumerate(udev_enumerate_new(udev.get()));

  udev_enumerate_add_match_subsystem(enumerate.get(), "usb");
  if (udev_enumerate_scan_devices(enumerate.get()) != 0) {
    return;
  }
  std::string bus_number =
      base::IntToString(libusb_get_bus_number(platform_device));
  std::string device_address =
      base::IntToString(libusb_get_device_address(platform_device));
  udev_list_entry* devices = udev_enumerate_get_list_entry(enumerate.get());
  for (udev_list_entry* i = devices; i != NULL;
       i = udev_list_entry_get_next(i)) {
    ScopedUdevDevicePtr device(
        udev_device_new_from_syspath(udev.get(), udev_list_entry_get_name(i)));
    if (device) {
      const char* value = udev_device_get_sysattr_value(device.get(), "busnum");
      if (!value || bus_number != value) {
        continue;
      }
      value = udev_device_get_sysattr_value(device.get(), "devnum");
      if (!value || device_address != value) {
        continue;
      }

      value = udev_device_get_sysattr_value(device.get(), "manufacturer");
      manufacturer_ = value ? value : "";
      value = udev_device_get_sysattr_value(device.get(), "product");
      product_ = value ? value : "";
      value = udev_device_get_sysattr_value(device.get(), "serial");
      serial_number_ = value ? value : "";
      break;
    }
  }
#endif
}

UsbDeviceImpl::~UsbDeviceImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (HandlesVector::iterator it = handles_.begin(); it != handles_.end();
       ++it) {
    (*it)->InternalClose();
  }
  STLClearObject(&handles_);
  libusb_unref_device(platform_device_);
}

#if defined(OS_CHROMEOS)

void UsbDeviceImpl::RequestUsbAccess(
    int interface_id,
    const base::Callback<void(bool success)>& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // ChromeOS builds on non-ChromeOS machines (dev) should not attempt to
  // use permission broker.
  if (base::SysInfo::IsRunningOnChromeOS()) {
    chromeos::PermissionBrokerClient* client =
        chromeos::DBusThreadManager::Get()->GetPermissionBrokerClient();
    DCHECK(client) << "Could not get permission broker client.";
    if (!client) {
      callback.Run(false);
      return;
    }

    ui_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&chromeos::PermissionBrokerClient::RequestUsbAccess,
                   base::Unretained(client),
                   vendor_id(),
                   product_id(),
                   interface_id,
                   base::Bind(&OnRequestUsbAccessReplied,
                              base::ThreadTaskRunnerHandle::Get(),
                              callback)));
  }
}

#endif

scoped_refptr<UsbDeviceHandle> UsbDeviceImpl::Open() {
  DCHECK(thread_checker_.CalledOnValidThread());
  PlatformUsbDeviceHandle handle;
  const int rv = libusb_open(platform_device_, &handle);
  if (LIBUSB_SUCCESS == rv) {
    GetConfiguration();
    if (!current_configuration_cached_) {
      return NULL;
    }
    scoped_refptr<UsbDeviceHandleImpl> device_handle =
        new UsbDeviceHandleImpl(context_, this, handle, current_configuration_);
    handles_.push_back(device_handle);
    return device_handle;
  } else {
    VLOG(1) << "Failed to open device: " << ConvertPlatformUsbErrorToString(rv);
    return NULL;
  }
}

bool UsbDeviceImpl::Close(scoped_refptr<UsbDeviceHandle> handle) {
  DCHECK(thread_checker_.CalledOnValidThread());

  for (HandlesVector::iterator it = handles_.begin(); it != handles_.end();
       ++it) {
    if (it->get() == handle.get()) {
      (*it)->InternalClose();
      handles_.erase(it);
      return true;
    }
  }
  return false;
}

const UsbConfigDescriptor& UsbDeviceImpl::GetConfiguration() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!current_configuration_cached_) {
    libusb_config_descriptor* platform_config;
    const int rv =
        libusb_get_active_config_descriptor(platform_device_, &platform_config);
    if (rv != LIBUSB_SUCCESS) {
      VLOG(1) << "Failed to get config descriptor: "
              << ConvertPlatformUsbErrorToString(rv);
      return current_configuration_;
    }

    current_configuration_.configuration_value =
        platform_config->bConfigurationValue;
    current_configuration_.self_powered =
        (platform_config->bmAttributes & 0x40) != 0;
    current_configuration_.remote_wakeup =
        (platform_config->bmAttributes & 0x20) != 0;
    current_configuration_.maximum_power = platform_config->MaxPower * 2;

    for (size_t i = 0; i < platform_config->bNumInterfaces; ++i) {
      const struct libusb_interface* platform_interface =
          &platform_config->interface[i];
      for (int j = 0; j < platform_interface->num_altsetting; ++j) {
        const struct libusb_interface_descriptor* platform_alt_setting =
            &platform_interface->altsetting[j];
        UsbInterfaceDescriptor interface;

        interface.interface_number = platform_alt_setting->bInterfaceNumber;
        interface.alternate_setting = platform_alt_setting->bAlternateSetting;
        interface.interface_class = platform_alt_setting->bInterfaceClass;
        interface.interface_subclass = platform_alt_setting->bInterfaceSubClass;
        interface.interface_protocol = platform_alt_setting->bInterfaceProtocol;

        for (size_t k = 0; k < platform_alt_setting->bNumEndpoints; ++k) {
          const struct libusb_endpoint_descriptor* platform_endpoint =
              &platform_alt_setting->endpoint[k];
          UsbEndpointDescriptor endpoint;

          endpoint.address = platform_endpoint->bEndpointAddress;
          endpoint.direction = GetDirection(platform_endpoint);
          endpoint.maximum_packet_size = platform_endpoint->wMaxPacketSize;
          endpoint.synchronization_type =
              GetSynchronizationType(platform_endpoint);
          endpoint.transfer_type = GetTransferType(platform_endpoint);
          endpoint.usage_type = GetUsageType(platform_endpoint);
          endpoint.polling_interval = platform_endpoint->bInterval;
          endpoint.extra_data = std::vector<uint8_t>(
              platform_endpoint->extra,
              platform_endpoint->extra + platform_endpoint->extra_length);

          interface.endpoints.push_back(endpoint);
        }

        interface.extra_data = std::vector<uint8_t>(
            platform_alt_setting->extra,
            platform_alt_setting->extra + platform_alt_setting->extra_length);

        current_configuration_.interfaces.push_back(interface);
      }
    }

    current_configuration_.extra_data = std::vector<uint8_t>(
        platform_config->extra,
        platform_config->extra + platform_config->extra_length);

    libusb_free_config_descriptor(platform_config);
    current_configuration_cached_ = true;
  }

  return current_configuration_;
}

bool UsbDeviceImpl::GetManufacturer(base::string16* manufacturer) {
  DCHECK(thread_checker_.CalledOnValidThread());

#if defined(USE_UDEV)
  if (manufacturer_.empty()) {
    return false;
  }
  *manufacturer = base::UTF8ToUTF16(manufacturer_);
  return true;
#else
  // This is a non-blocking call as libusb has the descriptor in memory.
  libusb_device_descriptor desc;
  const int rv = libusb_get_device_descriptor(platform_device_, &desc);
  if (rv != LIBUSB_SUCCESS) {
    VLOG(1) << "Failed to read device descriptor: "
            << ConvertPlatformUsbErrorToString(rv);
    return false;
  }

  if (desc.iManufacturer == 0) {
    return false;
  }

  scoped_refptr<UsbDeviceHandle> device_handle = Open();
  if (device_handle.get()) {
    return device_handle->GetStringDescriptor(desc.iManufacturer, manufacturer);
  }
  return false;
#endif
}

bool UsbDeviceImpl::GetProduct(base::string16* product) {
  DCHECK(thread_checker_.CalledOnValidThread());

#if defined(USE_UDEV)
  if (product_.empty()) {
    return false;
  }
  *product = base::UTF8ToUTF16(product_);
  return true;
#else
  // This is a non-blocking call as libusb has the descriptor in memory.
  libusb_device_descriptor desc;
  const int rv = libusb_get_device_descriptor(platform_device_, &desc);
  if (rv != LIBUSB_SUCCESS) {
    VLOG(1) << "Failed to read device descriptor: "
            << ConvertPlatformUsbErrorToString(rv);
    return false;
  }

  if (desc.iProduct == 0) {
    return false;
  }

  scoped_refptr<UsbDeviceHandle> device_handle = Open();
  if (device_handle.get()) {
    return device_handle->GetStringDescriptor(desc.iProduct, product);
  }
  return false;
#endif
}

bool UsbDeviceImpl::GetSerialNumber(base::string16* serial_number) {
  DCHECK(thread_checker_.CalledOnValidThread());

#if defined(USE_UDEV)
  if (serial_number_.empty()) {
    return false;
  }
  *serial_number = base::UTF8ToUTF16(serial_number_);
  return true;
#else
  // This is a non-blocking call as libusb has the descriptor in memory.
  libusb_device_descriptor desc;
  const int rv = libusb_get_device_descriptor(platform_device_, &desc);
  if (rv != LIBUSB_SUCCESS) {
    VLOG(1) << "Failed to read device descriptor: "
            << ConvertPlatformUsbErrorToString(rv);
    return false;
  }

  if (desc.iSerialNumber == 0) {
    return false;
  }

  scoped_refptr<UsbDeviceHandle> device_handle = Open();
  if (device_handle.get()) {
    return device_handle->GetStringDescriptor(desc.iSerialNumber,
                                              serial_number);
  }
  return false;
#endif
}

void UsbDeviceImpl::OnDisconnect() {
  DCHECK(thread_checker_.CalledOnValidThread());
  HandlesVector handles;
  swap(handles, handles_);
  for (HandlesVector::iterator it = handles.begin(); it != handles.end(); ++it)
    (*it)->InternalClose();
}

}  // namespace device

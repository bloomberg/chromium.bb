// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_service_impl.h"

#include <algorithm>
#include <set>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "components/device_event_log/device_event_log.h"
#include "device/usb/usb_error.h"
#include "third_party/libusb/src/libusb/libusb.h"

#if defined(OS_WIN)
#include <setupapi.h>
#include <usbiodef.h>

#include "base/strings/string_util.h"
#endif  // OS_WIN

#if defined(USE_UDEV)
#include "device/udev_linux/scoped_udev.h"
#endif  // USE_UDEV

namespace device {

namespace {

#if defined(USE_UDEV)

void ReadDeviceStrings(PlatformUsbDevice platform_device,
                       libusb_device_descriptor* descriptor,
                       base::string16* manufacturer_string,
                       base::string16* product_string,
                       base::string16* serial_number,
                       std::string* device_node) {
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

      value = udev_device_get_devnode(device.get());
      if (value) {
        *device_node = value;
      }
      value = udev_device_get_sysattr_value(device.get(), "manufacturer");
      if (value) {
        *manufacturer_string = base::UTF8ToUTF16(value);
      }
      value = udev_device_get_sysattr_value(device.get(), "product");
      if (value) {
        *product_string = base::UTF8ToUTF16(value);
      }
      value = udev_device_get_sysattr_value(device.get(), "serial");
      if (value) {
        *serial_number = base::UTF8ToUTF16(value);
      }
      break;
    }
  }
}

#else

uint16 ReadDeviceLanguage(PlatformUsbDeviceHandle handle) {
  uint16 language_id = 0x0409;
  uint8 buffer[256];
  int size =
      libusb_get_string_descriptor(handle, 0, 0, &buffer[0], sizeof(buffer));
  if (size < 0) {
    USB_LOG(EVENT) << "Failed to get supported string languages: "
                   << ConvertPlatformUsbErrorToString(size);
  } else if (size >= 4) {
    // Just pick the first supported language.
    language_id = buffer[2] | (buffer[3] << 8);
  } else {
    USB_LOG(EVENT) << "List of available string languages invalid.";
  }

  return language_id;
}

void ReadDeviceString(PlatformUsbDeviceHandle handle,
                      uint8 string_id,
                      uint16 language_id,
                      base::string16* string) {
  if (string_id == 0) {
    return;
  }

  uint8 buffer[256];
  int size = libusb_get_string_descriptor(handle, string_id, language_id,
                                          &buffer[0], sizeof(buffer));
  if (size < 0) {
    USB_LOG(EVENT) << "Failed to read string " << (int)string_id
                   << " from the device: "
                   << ConvertPlatformUsbErrorToString(size);
  } else if (size > 2) {
    *string = base::string16(reinterpret_cast<base::char16*>(&buffer[2]),
                             size / 2 - 1);
  } else {
    USB_LOG(EVENT) << "String descriptor " << string_id << " is invalid.";
  }
}

void ReadDeviceStrings(PlatformUsbDevice platform_device,
                       libusb_device_descriptor* descriptor,
                       base::string16* manufacturer_string,
                       base::string16* product_string,
                       base::string16* serial_number,
                       std::string* device_node) {
  if (descriptor->iManufacturer == 0 && descriptor->iProduct == 0 &&
      descriptor->iSerialNumber == 0) {
    // Don't bother distrubing the device if it doesn't have any string
    // descriptors we care about.
    return;
  }

  PlatformUsbDeviceHandle handle;
  int rv = libusb_open(platform_device, &handle);
  if (rv != LIBUSB_SUCCESS) {
    USB_LOG(EVENT) << "Failed to open device to read string descriptors: "
                   << ConvertPlatformUsbErrorToString(rv);
    return;
  }

  uint16 language_id = ReadDeviceLanguage(handle);
  ReadDeviceString(handle, descriptor->iManufacturer, language_id,
                   manufacturer_string);
  ReadDeviceString(handle, descriptor->iProduct, language_id, product_string);
  ReadDeviceString(handle, descriptor->iSerialNumber, language_id,
                   serial_number);
  libusb_close(handle);
}

#endif  // USE_UDEV

#if defined(OS_WIN)

// Wrapper around a HDEVINFO that automatically destroys it.
class ScopedDeviceInfoList {
 public:
  explicit ScopedDeviceInfoList(HDEVINFO handle) : handle_(handle) {}

  ~ScopedDeviceInfoList() {
    if (valid()) {
      SetupDiDestroyDeviceInfoList(handle_);
    }
  }

  bool valid() { return handle_ != INVALID_HANDLE_VALUE; }

  HDEVINFO get() { return handle_; }

 private:
  HDEVINFO handle_;

  DISALLOW_COPY_AND_ASSIGN(ScopedDeviceInfoList);
};

// Wrapper around an SP_DEVINFO_DATA that initializes it properly and
// automatically deletes it.
class ScopedDeviceInfo {
 public:
  ScopedDeviceInfo() {
    memset(&dev_info_data_, 0, sizeof(dev_info_data_));
    dev_info_data_.cbSize = sizeof(dev_info_data_);
  }

  ~ScopedDeviceInfo() {
    if (dev_info_set_ != INVALID_HANDLE_VALUE) {
      SetupDiDeleteDeviceInfo(dev_info_set_, &dev_info_data_);
    }
  }

  // Once the SP_DEVINFO_DATA has been populated it must be freed using the
  // HDEVINFO it was created from.
  void set_valid(HDEVINFO dev_info_set) {
    DCHECK(dev_info_set_ == INVALID_HANDLE_VALUE);
    DCHECK(dev_info_set != INVALID_HANDLE_VALUE);
    dev_info_set_ = dev_info_set;
  }

  PSP_DEVINFO_DATA get() { return &dev_info_data_; }

 private:
  HDEVINFO dev_info_set_ = INVALID_HANDLE_VALUE;
  SP_DEVINFO_DATA dev_info_data_;
};

bool IsWinUsbInterface(const std::string& device_path) {
  ScopedDeviceInfoList dev_info_list(SetupDiCreateDeviceInfoList(NULL, NULL));
  if (!dev_info_list.valid()) {
    USB_PLOG(ERROR) << "Failed to create a device information set";
    return false;
  }

  // This will add the device to |dev_info_list| so we can query driver info.
  if (!SetupDiOpenDeviceInterfaceA(dev_info_list.get(), device_path.c_str(), 0,
                                   NULL)) {
    USB_PLOG(ERROR) << "Failed to get device interface data for "
                    << device_path;
    return false;
  }

  ScopedDeviceInfo dev_info;
  if (!SetupDiEnumDeviceInfo(dev_info_list.get(), 0, dev_info.get())) {
    USB_PLOG(ERROR) << "Failed to get device info for " << device_path;
    return false;
  }
  dev_info.set_valid(dev_info_list.get());

  DWORD reg_data_type;
  BYTE buffer[256];
  if (!SetupDiGetDeviceRegistryPropertyA(dev_info_list.get(), dev_info.get(),
                                         SPDRP_SERVICE, &reg_data_type,
                                         &buffer[0], sizeof buffer, NULL)) {
    USB_PLOG(ERROR) << "Failed to get device service property";
    return false;
  }
  if (reg_data_type != REG_SZ) {
    USB_LOG(ERROR) << "Unexpected data type for driver service: "
                   << reg_data_type;
    return false;
  }

  USB_LOG(DEBUG) << "Driver for " << device_path << " is " << buffer << ".";
  if (base::strncasecmp("WinUSB", (const char*)&buffer[0], sizeof "WinUSB") ==
      0) {
    return true;
  }
  return false;
}

#endif  // OS_WIN

}  // namespace

// static
UsbService* UsbServiceImpl::Create(
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner) {
  PlatformUsbContext context = NULL;
  const int rv = libusb_init(&context);
  if (rv != LIBUSB_SUCCESS) {
    USB_LOG(ERROR) << "Failed to initialize libusb: "
                   << ConvertPlatformUsbErrorToString(rv);
    return nullptr;
  }
  if (!context) {
    return nullptr;
  }

  return new UsbServiceImpl(context, blocking_task_runner);
}

UsbServiceImpl::UsbServiceImpl(
    PlatformUsbContext context,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
    : context_(new UsbContext(context)),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      blocking_task_runner_(blocking_task_runner),
#if defined(OS_WIN)
      device_observer_(this),
#endif
      weak_factory_(this) {
  base::MessageLoop::current()->AddDestructionObserver(this);

  int rv = libusb_hotplug_register_callback(
      context_->context(),
      static_cast<libusb_hotplug_event>(LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED |
                                        LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT),
      LIBUSB_HOTPLUG_ENUMERATE, LIBUSB_HOTPLUG_MATCH_ANY,
      LIBUSB_HOTPLUG_MATCH_ANY, LIBUSB_HOTPLUG_MATCH_ANY,
      &UsbServiceImpl::HotplugCallback, this, &hotplug_handle_);
  if (rv == LIBUSB_SUCCESS) {
    hotplug_enabled_ = true;

    // libusb will call the hotplug callback for each device currently
    // enumerated. Once this is complete enumeration_ready_ can be set to true
    // but we must first wait for any tasks posted to blocking_task_runner_ to
    // complete.
    blocking_task_runner_->PostTaskAndReply(
        FROM_HERE, base::Bind(&base::DoNothing),
        base::Bind(&UsbServiceImpl::RefreshDevicesComplete,
                   weak_factory_.GetWeakPtr(), nullptr, 0));
  } else {
    RefreshDevices("");
#if defined(OS_WIN)
    DeviceMonitorWin* device_monitor = DeviceMonitorWin::GetForAllInterfaces();
    if (device_monitor) {
      device_observer_.Add(device_monitor);
    }
#endif  // OS_WIN
  }
}

UsbServiceImpl::~UsbServiceImpl() {
  base::MessageLoop::current()->RemoveDestructionObserver(this);

  if (hotplug_enabled_) {
    libusb_hotplug_deregister_callback(context_->context(), hotplug_handle_);
  }
  for (const auto& map_entry : devices_) {
    map_entry.second->OnDisconnect();
  }
}

scoped_refptr<UsbDevice> UsbServiceImpl::GetDevice(const std::string& guid) {
  DCHECK(CalledOnValidThread());
  DeviceMap::iterator it = devices_.find(guid);
  if (it != devices_.end()) {
    return it->second;
  }
  return NULL;
}

void UsbServiceImpl::GetDevices(const GetDevicesCallback& callback) {
  DCHECK(CalledOnValidThread());

  if (!enumeration_ready_) {
    // On startup wait for the first enumeration,
    pending_enumerations_.push_back(callback);
  } else if (hotplug_enabled_) {
    // The device list is updated live when hotplug events are supported.
    std::vector<scoped_refptr<UsbDevice>> devices;
    for (const auto& map_entry : devices_) {
      devices.push_back(map_entry.second);
    }
    callback.Run(devices);
  } else {
    // Only post one re-enumeration task at a time.
    if (pending_enumerations_.empty()) {
      RefreshDevices("");
    }
    pending_enumerations_.push_back(callback);
  }
}

#if defined(OS_WIN)

void UsbServiceImpl::OnDeviceAdded(const GUID& class_guid,
                                   const std::string& device_path) {
  // Only the root node of a composite USB device has the class GUID
  // GUID_DEVINTERFACE_USB_DEVICE but we want to wait until WinUSB is loaded.
  // This first pass filter will catch anything that's sitting on the USB bus
  // (including devices on 3rd party USB controllers) to avoid the more
  // expensive driver check that needs to be done on the FILE thread.
  if (device_path.find("usb") != std::string::npos) {
    RefreshDevices(device_path);
  }
}

void UsbServiceImpl::OnDeviceRemoved(const GUID& class_guid,
                                     const std::string& device_path) {
  // The root USB device node is removed last
  if (class_guid == GUID_DEVINTERFACE_USB_DEVICE) {
    RefreshDevices("");
  }
}

#endif  // OS_WIN

void UsbServiceImpl::WillDestroyCurrentMessageLoop() {
  DCHECK(CalledOnValidThread());
  delete this;
}

void UsbServiceImpl::RefreshDevices(const std::string& new_device_path) {
  DCHECK(CalledOnValidThread());

  std::set<PlatformUsbDevice> current_devices;
  for (const auto& map_entry : platform_devices_) {
    current_devices.insert(map_entry.first);
  }
  blocking_task_runner_->PostTask(
      FROM_HERE, base::Bind(&UsbServiceImpl::RefreshDevicesOnBlockingThread,
                            weak_factory_.GetWeakPtr(), new_device_path,
                            task_runner_, context_, current_devices));
}

// static
void UsbServiceImpl::RefreshDevicesOnBlockingThread(
    base::WeakPtr<UsbServiceImpl> usb_service,
    const std::string& new_device_path,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    scoped_refptr<UsbContext> usb_context,
    const std::set<PlatformUsbDevice>& previous_devices) {
  if (!new_device_path.empty()) {
#if defined(OS_WIN)
    if (!IsWinUsbInterface(new_device_path)) {
      // Wait to call libusb_get_device_list until libusb will be able to find
      // a WinUSB interface for the device.
      return;
    }
#endif  // defined(OS_WIN)
  }

  libusb_device** platform_devices = NULL;
  const ssize_t device_count =
      libusb_get_device_list(usb_context->context(), &platform_devices);
  if (device_count < 0) {
    USB_LOG(ERROR) << "Failed to get device list: "
                   << ConvertPlatformUsbErrorToString(device_count);
    task_runner->PostTask(FROM_HERE,
                          base::Bind(&UsbServiceImpl::RefreshDevicesComplete,
                                     usb_service, nullptr, 0));
    return;
  }

  // Find new devices.
  for (ssize_t i = 0; i < device_count; ++i) {
    PlatformUsbDevice platform_device = platform_devices[i];
    if (previous_devices.find(platform_device) == previous_devices.end()) {
      libusb_ref_device(platform_device);
      AddDeviceOnBlockingThread(usb_service, task_runner, platform_device);
    }
  }

  // |platform_devices| will be freed in this callback.
  task_runner->PostTask(
      FROM_HERE, base::Bind(&UsbServiceImpl::RefreshDevicesComplete,
                            usb_service, platform_devices, device_count));
}

// static
void UsbServiceImpl::AddDeviceOnBlockingThread(
    base::WeakPtr<UsbServiceImpl> usb_service,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    PlatformUsbDevice platform_device) {
  libusb_device_descriptor descriptor;
  int rv = libusb_get_device_descriptor(platform_device, &descriptor);
  if (rv == LIBUSB_SUCCESS) {
    base::string16 manufacturer_string;
    base::string16 product_string;
    base::string16 serial_number;
    std::string device_node;
    ReadDeviceStrings(platform_device, &descriptor, &manufacturer_string,
                      &product_string, &serial_number, &device_node);

    task_runner->PostTask(
        FROM_HERE, base::Bind(&UsbServiceImpl::AddDevice, usb_service,
                              platform_device, descriptor.idVendor,
                              descriptor.idProduct, manufacturer_string,
                              product_string, serial_number, device_node));
  } else {
    USB_LOG(EVENT) << "Failed to get device descriptor: "
                   << ConvertPlatformUsbErrorToString(rv);
    libusb_unref_device(platform_device);
  }
}

void UsbServiceImpl::RefreshDevicesComplete(libusb_device** platform_devices,
                                            ssize_t device_count) {
  if (platform_devices) {
    // Mark devices seen in this enumeration.
    for (ssize_t i = 0; i < device_count; ++i) {
      const PlatformDeviceMap::iterator it =
          platform_devices_.find(platform_devices[i]);
      if (it != platform_devices_.end()) {
        it->second->set_visited(true);
      }
    }

    // Remove devices not seen in this enumeration.
    for (PlatformDeviceMap::iterator it = platform_devices_.begin();
         it != platform_devices_.end();
         /* incremented internally */) {
      PlatformDeviceMap::iterator current = it++;
      const scoped_refptr<UsbDeviceImpl>& device = current->second;
      if (device->was_visited()) {
        device->set_visited(false);
      } else {
        RemoveDevice(device);
      }
    }

    libusb_free_device_list(platform_devices, true);
  }

  enumeration_ready_ = true;

  if (!pending_enumerations_.empty()) {
    std::vector<scoped_refptr<UsbDevice>> devices;
    for (const auto& map_entry : devices_) {
      devices.push_back(map_entry.second);
    }

    std::vector<GetDevicesCallback> pending_enumerations;
    pending_enumerations.swap(pending_enumerations_);
    for (const GetDevicesCallback& callback : pending_enumerations) {
      callback.Run(devices);
    }
  }
}

void UsbServiceImpl::AddDevice(PlatformUsbDevice platform_device,
                               uint16 vendor_id,
                               uint16 product_id,
                               base::string16 manufacturer_string,
                               base::string16 product_string,
                               base::string16 serial_number,
                               std::string device_node) {
  scoped_refptr<UsbDeviceImpl> device(new UsbDeviceImpl(
      context_, platform_device, vendor_id, product_id, manufacturer_string,
      product_string, serial_number, device_node, blocking_task_runner_));

  platform_devices_[platform_device] = device;
  DCHECK(!ContainsKey(devices_, device->guid()));
  devices_[device->guid()] = device;

  USB_LOG(USER) << "USB device added: vendor=" << device->vendor_id() << " \""
                << device->manufacturer_string()
                << "\", product=" << device->product_id() << " \""
                << device->product_string() << "\", serial=\""
                << device->serial_number() << "\", guid=" << device->guid();

  if (enumeration_ready_) {
    NotifyDeviceAdded(device);
  }

  libusb_unref_device(platform_device);
}

void UsbServiceImpl::RemoveDevice(scoped_refptr<UsbDeviceImpl> device) {
  platform_devices_.erase(device->platform_device());
  devices_.erase(device->guid());

  USB_LOG(USER) << "USB device removed: guid=" << device->guid();

  NotifyDeviceRemoved(device);
  device->OnDisconnect();
}

// static
int LIBUSB_CALL UsbServiceImpl::HotplugCallback(libusb_context* context,
                                                PlatformUsbDevice device,
                                                libusb_hotplug_event event,
                                                void* user_data) {
  // It is safe to access the UsbServiceImpl* here because libusb takes a lock
  // around registering, deregistering and calling hotplug callback functions
  // and so guarantees that this function will not be called by the event
  // processing thread after it has been deregistered.
  UsbServiceImpl* self = reinterpret_cast<UsbServiceImpl*>(user_data);
  switch (event) {
    case LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED:
      libusb_ref_device(device);  // Released in OnPlatformDeviceAdded.
      if (self->task_runner_->BelongsToCurrentThread()) {
        self->OnPlatformDeviceAdded(device);
      } else {
        self->task_runner_->PostTask(
            FROM_HERE, base::Bind(&UsbServiceImpl::OnPlatformDeviceAdded,
                                  base::Unretained(self), device));
      }
      break;
    case LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT:
      libusb_ref_device(device);  // Released in OnPlatformDeviceRemoved.
      if (self->task_runner_->BelongsToCurrentThread()) {
        self->OnPlatformDeviceRemoved(device);
      } else {
        self->task_runner_->PostTask(
            FROM_HERE, base::Bind(&UsbServiceImpl::OnPlatformDeviceRemoved,
                                  base::Unretained(self), device));
      }
      break;
    default:
      NOTREACHED();
  }

  return 0;
}

void UsbServiceImpl::OnPlatformDeviceAdded(PlatformUsbDevice platform_device) {
  DCHECK(CalledOnValidThread());
  DCHECK(!ContainsKey(platform_devices_, platform_device));
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&UsbServiceImpl::AddDeviceOnBlockingThread,
                 weak_factory_.GetWeakPtr(), task_runner_, platform_device));

  // libusb_unref_device(platform_device) is called by the task above.
}

void UsbServiceImpl::OnPlatformDeviceRemoved(
    PlatformUsbDevice platform_device) {
  DCHECK(CalledOnValidThread());
  PlatformDeviceMap::iterator it = platform_devices_.find(platform_device);
  if (it != platform_devices_.end()) {
    scoped_refptr<UsbDeviceImpl> device = it->second;
    // Serialize with calls to AddDeviceOnBlockingThread.
    blocking_task_runner_->PostTaskAndReply(
        FROM_HERE, base::Bind(&base::DoNothing),
        base::Bind(&UsbServiceImpl::RemoveDevice, weak_factory_.GetWeakPtr(),
                   device));
  } else {
    NOTREACHED();
  }

  libusb_unref_device(platform_device);
}

}  // namespace device

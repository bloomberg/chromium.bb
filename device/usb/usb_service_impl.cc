// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_service_impl.h"

#include <set>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/thread_task_runner_handle.h"
#include "components/device_event_log/device_event_log.h"
#include "device/usb/usb_error.h"

#if defined(OS_WIN)
#include <setupapi.h>
#include <usbiodef.h>

#include "base/scoped_observer.h"
#include "base/strings/string_util.h"
#include "device/core/device_monitor_win.h"
#endif  // OS_WIN

namespace device {

#if defined(OS_WIN)

namespace {

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

}  // namespace

// This class lives on the application main thread so that it can listen for
// device change notification window messages. It registers for notifications
// that may indicate new devices that the UsbService will enumerate.
class UsbServiceImpl::UIThreadHelper final
    : private DeviceMonitorWin::Observer {
 public:
  UIThreadHelper(base::WeakPtr<UsbServiceImpl> usb_service)
      : task_runner_(base::ThreadTaskRunnerHandle::Get()),
        usb_service_(usb_service),
        device_observer_(this) {}

  ~UIThreadHelper() {}

  void Start() {
    DeviceMonitorWin* device_monitor = DeviceMonitorWin::GetForAllInterfaces();
    if (device_monitor) {
      device_observer_.Add(device_monitor);
    }
  }

 private:
  void OnDeviceAdded(const GUID& class_guid,
                     const std::string& device_path) override {
    // Only the root node of a composite USB device has the class GUID
    // GUID_DEVINTERFACE_USB_DEVICE but we want to wait until WinUSB is loaded.
    // This first pass filter will catch anything that's sitting on the USB bus
    // (including devices on 3rd party USB controllers) to avoid the more
    // expensive driver check that needs to be done on the FILE thread.
    if (device_path.find("usb") != std::string::npos) {
      task_runner_->PostTask(
          FROM_HERE, base::Bind(&UsbServiceImpl::RefreshDevicesIfWinUsbDevice,
                                usb_service_, device_path));
    }
  }

  void OnDeviceRemoved(const GUID& class_guid,
                       const std::string& device_path) override {
    // The root USB device node is removed last
    if (class_guid == GUID_DEVINTERFACE_USB_DEVICE) {
      task_runner_->PostTask(
          FROM_HERE, base::Bind(&UsbServiceImpl::RefreshDevices, usb_service_));
    }
  }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtr<UsbServiceImpl> usb_service_;
  ScopedObserver<DeviceMonitorWin, DeviceMonitorWin::Observer> device_observer_;
};

#endif  // OS_WIN

// static
UsbService* UsbServiceImpl::Create(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
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

  return new UsbServiceImpl(context, ui_task_runner);
}

scoped_refptr<UsbDevice> UsbServiceImpl::GetDeviceById(uint32 unique_id) {
  DCHECK(CalledOnValidThread());
  RefreshDevices();
  DeviceMap::iterator it = devices_.find(unique_id);
  if (it != devices_.end()) {
    return it->second;
  }
  return NULL;
}

void UsbServiceImpl::GetDevices(
    std::vector<scoped_refptr<UsbDevice> >* devices) {
  DCHECK(CalledOnValidThread());
  STLClearObject(devices);

  if (!hotplug_enabled_) {
    RefreshDevices();
  }

  for (const auto& map_entry : devices_) {
    devices->push_back(map_entry.second);
  }
}

UsbServiceImpl::UsbServiceImpl(
    PlatformUsbContext context,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : context_(new UsbContext(context)),
      ui_task_runner_(ui_task_runner),
      next_unique_id_(0),
      hotplug_enabled_(false),
      weak_factory_(this) {
  task_runner_ = base::ThreadTaskRunnerHandle::Get();
  int rv = libusb_hotplug_register_callback(
      context_->context(),
      static_cast<libusb_hotplug_event>(LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED |
                                        LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT),
      LIBUSB_HOTPLUG_ENUMERATE, LIBUSB_HOTPLUG_MATCH_ANY,
      LIBUSB_HOTPLUG_MATCH_ANY, LIBUSB_HOTPLUG_MATCH_ANY,
      &UsbServiceImpl::HotplugCallback, this, &hotplug_handle_);
  if (rv == LIBUSB_SUCCESS) {
    hotplug_enabled_ = true;
  } else {
#if defined(OS_WIN)
    ui_thread_helper_ = new UIThreadHelper(weak_factory_.GetWeakPtr());
    ui_task_runner_->PostTask(FROM_HERE,
                              base::Bind(&UIThreadHelper::Start,
                                         base::Unretained(ui_thread_helper_)));
#endif  // OS_WIN
  }
}

UsbServiceImpl::~UsbServiceImpl() {
  if (hotplug_enabled_) {
    libusb_hotplug_deregister_callback(context_->context(), hotplug_handle_);
  }
#if defined(OS_WIN)
  if (ui_thread_helper_) {
    ui_task_runner_->DeleteSoon(FROM_HERE, ui_thread_helper_);
  }
#endif  // OS_WIN
  for (const auto& map_entry : devices_) {
    map_entry.second->OnDisconnect();
  }
}

void UsbServiceImpl::RefreshDevices() {
  DCHECK(CalledOnValidThread());

  libusb_device** platform_devices = NULL;
  const ssize_t device_count =
      libusb_get_device_list(context_->context(), &platform_devices);
  if (device_count < 0) {
    USB_LOG(ERROR) << "Failed to get device list: "
                   << ConvertPlatformUsbErrorToString(device_count);
  }

  std::set<UsbDevice*> connected_devices;
  std::vector<PlatformUsbDevice> disconnected_devices;

  // Populates new devices.
  for (ssize_t i = 0; i < device_count; ++i) {
    if (!ContainsKey(platform_devices_, platform_devices[i])) {
      scoped_refptr<UsbDeviceImpl> new_device = AddDevice(platform_devices[i]);
      if (new_device) {
        connected_devices.insert(new_device.get());
      }
    } else {
      connected_devices.insert(platform_devices_[platform_devices[i]].get());
    }
  }

  // Find disconnected devices.
  for (const auto& map_entry : platform_devices_) {
    PlatformUsbDevice platform_device = map_entry.first;
    scoped_refptr<UsbDeviceImpl> device = map_entry.second;
    if (!ContainsKey(connected_devices, device.get())) {
      disconnected_devices.push_back(platform_device);
      devices_.erase(device->unique_id());

      NotifyDeviceRemoved(device);
      device->OnDisconnect();
    }
  }

  // Remove disconnected devices from platform_devices_.
  for (const PlatformUsbDevice& platform_device : disconnected_devices) {
    // UsbDevice will be destroyed after this. The corresponding
    // PlatformUsbDevice will be unref'ed during this process.
    platform_devices_.erase(platform_device);
  }

  libusb_free_device_list(platform_devices, true);
}

#if defined(OS_WIN)
void UsbServiceImpl::RefreshDevicesIfWinUsbDevice(
    const std::string& device_path) {
  ScopedDeviceInfoList dev_info_list(SetupDiCreateDeviceInfoList(NULL, NULL));
  if (!dev_info_list.valid()) {
    USB_PLOG(ERROR) << "Failed to create a device information set";
    return;
  }

  // This will add the device to |dev_info_list| so we can query driver info.
  if (!SetupDiOpenDeviceInterfaceA(dev_info_list.get(), device_path.c_str(), 0,
                                   NULL)) {
    USB_PLOG(ERROR) << "Failed to get device interface data for "
                    << device_path;
    return;
  }

  ScopedDeviceInfo dev_info;
  if (!SetupDiEnumDeviceInfo(dev_info_list.get(), 0, dev_info.get())) {
    USB_PLOG(ERROR) << "Failed to get device info for " << device_path;
    return;
  }
  dev_info.set_valid(dev_info_list.get());

  DWORD reg_data_type;
  BYTE buffer[256];
  if (!SetupDiGetDeviceRegistryPropertyA(dev_info_list.get(), dev_info.get(),
                                         SPDRP_SERVICE, &reg_data_type,
                                         &buffer[0], sizeof buffer, NULL)) {
    USB_PLOG(ERROR) << "Failed to get device service property";
    return;
  }
  if (reg_data_type != REG_SZ) {
    USB_LOG(ERROR) << "Unexpected data type for driver service: "
                   << reg_data_type;
    return;
  }

  USB_LOG(DEBUG) << "Driver for " << device_path << " is " << buffer << ".";
  if (base::strncasecmp("WinUSB", (const char*)&buffer[0], sizeof "WinUSB") ==
      0) {
    RefreshDevices();
  }
}
#endif  // OS_WIN

scoped_refptr<UsbDeviceImpl> UsbServiceImpl::AddDevice(
    PlatformUsbDevice platform_device) {
  libusb_device_descriptor descriptor;
  int rv = libusb_get_device_descriptor(platform_device, &descriptor);
  if (rv == LIBUSB_SUCCESS) {
    uint32 unique_id;
    do {
      unique_id = ++next_unique_id_;
    } while (devices_.find(unique_id) != devices_.end());

    scoped_refptr<UsbDeviceImpl> new_device(new UsbDeviceImpl(
        context_, ui_task_runner_, platform_device, descriptor.idVendor,
        descriptor.idProduct, unique_id));
    platform_devices_[platform_device] = new_device;
    devices_[unique_id] = new_device;
    NotifyDeviceAdded(new_device);
    return new_device;
  } else {
    USB_LOG(EVENT) << "Failed to get device descriptor: "
                   << ConvertPlatformUsbErrorToString(rv);
    return nullptr;
  }
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
      libusb_ref_device(device);  // Released in OnDeviceAdded.
      if (self->task_runner_->BelongsToCurrentThread()) {
        self->OnDeviceAdded(device);
      } else {
        self->task_runner_->PostTask(
            FROM_HERE, base::Bind(&UsbServiceImpl::OnDeviceAdded,
                                  base::Unretained(self), device));
      }
      break;
    case LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT:
      libusb_ref_device(device);  // Released in OnDeviceRemoved.
      if (self->task_runner_->BelongsToCurrentThread()) {
        self->OnDeviceRemoved(device);
      } else {
        self->task_runner_->PostTask(
            FROM_HERE, base::Bind(&UsbServiceImpl::OnDeviceRemoved,
                                  base::Unretained(self), device));
      }
      break;
    default:
      NOTREACHED();
  }

  return 0;
}

void UsbServiceImpl::OnDeviceAdded(PlatformUsbDevice platform_device) {
  DCHECK(CalledOnValidThread());
  DCHECK(!ContainsKey(platform_devices_, platform_device));

  AddDevice(platform_device);
  libusb_unref_device(platform_device);
}

void UsbServiceImpl::OnDeviceRemoved(PlatformUsbDevice platform_device) {
  DCHECK(CalledOnValidThread());

  PlatformDeviceMap::iterator it = platform_devices_.find(platform_device);
  if (it != platform_devices_.end()) {
    scoped_refptr<UsbDeviceImpl> device = it->second;
    DeviceMap::iterator dev_it = devices_.find(device->unique_id());
    if (dev_it != devices_.end()) {
      devices_.erase(dev_it);
    } else {
      NOTREACHED();
    }
    platform_devices_.erase(it);

    NotifyDeviceRemoved(device);
    device->OnDisconnect();
  } else {
    NOTREACHED();
  }

  libusb_unref_device(platform_device);
}

}  // namespace device

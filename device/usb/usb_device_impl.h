// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_USB_DEVICE_IMPL_H_
#define DEVICE_USB_USB_DEVICE_IMPL_H_

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "device/usb/usb_descriptors.h"
#include "device/usb/usb_device.h"
#include "device/usb/webusb_descriptors.h"

struct libusb_device;
struct libusb_config_descriptor;
struct libusb_device_handle;

namespace base {
class SequencedTaskRunner;
}

namespace dbus {
class FileDescriptor;
}

namespace device {

class UsbDeviceHandleImpl;
class UsbContext;

typedef struct libusb_device* PlatformUsbDevice;
typedef struct libusb_config_descriptor* PlatformUsbConfigDescriptor;
typedef struct libusb_device_handle* PlatformUsbDeviceHandle;

class UsbDeviceImpl : public UsbDevice {
 public:
// UsbDevice implementation:
#if defined(OS_CHROMEOS)
  void CheckUsbAccess(const ResultCallback& callback) override;
#endif  // OS_CHROMEOS
  void Open(const OpenCallback& callback) override;
  const UsbConfigDescriptor* GetActiveConfiguration() override;

  // These functions are used during enumeration only. The values must not
  // change during the object's lifetime.
  void set_manufacturer_string(const base::string16& value) {
    manufacturer_string_ = value;
  }
  void set_product_string(const base::string16& value) {
    product_string_ = value;
  }
  void set_serial_number(const base::string16& value) {
    serial_number_ = value;
  }
  void set_device_path(const std::string& value) { device_path_ = value; }
  void set_webusb_allowed_origins(
      scoped_ptr<WebUsbAllowedOrigins> allowed_origins) {
    webusb_allowed_origins_ = std::move(allowed_origins);
  }
  void set_webusb_landing_page(const GURL& url) { webusb_landing_page_ = url; }

  PlatformUsbDevice platform_device() const { return platform_device_; }

 protected:
  friend class UsbServiceImpl;
  friend class UsbDeviceHandleImpl;

  // Called by UsbServiceImpl only;
  UsbDeviceImpl(scoped_refptr<UsbContext> context,
                PlatformUsbDevice platform_device,
                uint16_t vendor_id,
                uint16_t product_id,
                scoped_refptr<base::SequencedTaskRunner> blocking_task_runner);

  ~UsbDeviceImpl() override;

  // Called only by UsbServiceImpl.
  void set_visited(bool visited) { visited_ = visited; }
  bool was_visited() const { return visited_; }
  void OnDisconnect();
  void ReadAllConfigurations();

  // Called by UsbDeviceHandleImpl.
  void Close(scoped_refptr<UsbDeviceHandle> handle);
  void RefreshActiveConfiguration();

 private:
  void GetAllConfigurations();
#if defined(OS_CHROMEOS)
  void OnOpenRequestComplete(const OpenCallback& callback,
                             dbus::FileDescriptor fd);
  void OpenOnBlockingThreadWithFd(dbus::FileDescriptor fd,
                                  const OpenCallback& callback);
#endif
  void OpenOnBlockingThread(const OpenCallback& callback);
  void Opened(PlatformUsbDeviceHandle platform_handle,
              const OpenCallback& callback);

  base::ThreadChecker thread_checker_;
  PlatformUsbDevice platform_device_;
  bool visited_ = false;

  // On Chrome OS device path is necessary to request access from the permission
  // broker.
  std::string device_path_;

  // The current device configuration descriptor. May be null if the device is
  // in an unconfigured state; if not null, it is a pointer to one of the
  // items at UsbDevice::configurations_.
  const UsbConfigDescriptor* active_configuration_ = nullptr;

  // Retain the context so that it will not be released before UsbDevice.
  scoped_refptr<UsbContext> context_;

  // Opened handles.
  typedef std::vector<scoped_refptr<UsbDeviceHandleImpl> > HandlesVector;
  HandlesVector handles_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(UsbDeviceImpl);
};

}  // namespace device

#endif  // DEVICE_USB_USB_DEVICE_IMPL_H_

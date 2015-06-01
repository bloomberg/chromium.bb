// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_USB_DEVICE_IMPL_H_
#define DEVICE_USB_USB_DEVICE_IMPL_H_

#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/threading/thread_checker.h"
#include "device/usb/usb_descriptors.h"
#include "device/usb/usb_device.h"

struct libusb_device;
struct libusb_config_descriptor;
struct libusb_device_handle;

namespace base {
class SequencedTaskRunner;
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
  // Only overridden on Chrome OS.
  void CheckUsbAccess(const ResultCallback& callback) override;
  void RequestUsbAccess(int interface_id,
                        const ResultCallback& callback) override;
#endif  // OS_CHROMEOS
  void Open(const OpenCallback& callback) override;
  bool Close(scoped_refptr<UsbDeviceHandle> handle) override;
  const UsbConfigDescriptor* GetConfiguration() override;

 protected:
  friend class UsbServiceImpl;
  friend class UsbDeviceHandleImpl;

  // Called by UsbServiceImpl only;
  UsbDeviceImpl(scoped_refptr<UsbContext> context,
                PlatformUsbDevice platform_device,
                uint16 vendor_id,
                uint16 product_id,
                const base::string16& manufacturer_string,
                const base::string16& product_string,
                const base::string16& serial_number,
                const std::string& device_node,
                scoped_refptr<base::SequencedTaskRunner> blocking_task_runner);

  ~UsbDeviceImpl() override;

  // Called only by UsbServiceImpl.
  PlatformUsbDevice platform_device() const { return platform_device_; }
  void set_visited(bool visited) { visited_ = visited; }
  bool was_visited() const { return visited_; }
  void OnDisconnect();

  // Called by UsbDeviceHandleImpl.
  void RefreshConfiguration();

 private:
  void OpenOnBlockingThread(const OpenCallback& callback);
  void Opened(PlatformUsbDeviceHandle platform_handle,
              const OpenCallback& callback);

  base::ThreadChecker thread_checker_;
  PlatformUsbDevice platform_device_;
  bool visited_ = false;

#if defined(OS_CHROMEOS)
  // On Chrome OS save the devnode string for requesting path access from
  // permission broker.
  std::string devnode_;
#endif

  // The current device configuration descriptor. May be null if the device is
  // in an unconfigured state.
  scoped_ptr<UsbConfigDescriptor> configuration_;

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

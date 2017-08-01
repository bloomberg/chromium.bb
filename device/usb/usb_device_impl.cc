// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_device_impl.h"

#include <fcntl.h>
#include <stddef.h>

#include <algorithm>

#include "base/bind.h"
#include "base/location.h"
#include "base/posix/eintr_wrapper.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/device_event_log/device_event_log.h"
#include "device/usb/usb_context.h"
#include "device/usb/usb_descriptors.h"
#include "device/usb/usb_device_handle_impl.h"
#include "device/usb/usb_error.h"
#include "device/usb/usb_service.h"
#include "third_party/libusb/src/libusb/libusb.h"

namespace device {

UsbDeviceImpl::UsbDeviceImpl(scoped_refptr<UsbContext> context,
                             PlatformUsbDevice platform_device,
                             const libusb_device_descriptor& descriptor)
    : UsbDevice(descriptor.bcdUSB,
                descriptor.bDeviceClass,
                descriptor.bDeviceSubClass,
                descriptor.bDeviceProtocol,
                descriptor.idVendor,
                descriptor.idProduct,
                descriptor.bcdDevice,
                base::string16(),
                base::string16(),
                base::string16()),
      platform_device_(platform_device),
      context_(context) {
  CHECK(platform_device) << "platform_device cannot be NULL";
  libusb_ref_device(platform_device);
  ReadAllConfigurations();
  RefreshActiveConfiguration();
}

UsbDeviceImpl::~UsbDeviceImpl() {
  // The destructor must be safe to call from any thread.
  libusb_unref_device(platform_device_);
}

void UsbDeviceImpl::Open(OpenCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner =
      UsbService::CreateBlockingTaskRunner();
  blocking_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&UsbDeviceImpl::OpenOnBlockingThread, this,
                     std::move(callback), base::ThreadTaskRunnerHandle::Get(),
                     blocking_task_runner));
}

void UsbDeviceImpl::ReadAllConfigurations() {
  libusb_device_descriptor device_descriptor;
  int rv = libusb_get_device_descriptor(platform_device_, &device_descriptor);
  if (rv == LIBUSB_SUCCESS) {
    for (uint8_t i = 0; i < device_descriptor.bNumConfigurations; ++i) {
      unsigned char* buffer;
      rv = libusb_get_raw_config_descriptor(platform_device_, i, &buffer);
      if (rv < 0) {
        USB_LOG(EVENT) << "Failed to get config descriptor: "
                       << ConvertPlatformUsbErrorToString(rv);
        continue;
      }

      if (!descriptor_.Parse(std::vector<uint8_t>(buffer, buffer + rv)))
        USB_LOG(EVENT) << "Config descriptor index " << i << " was corrupt.";
      free(buffer);
    }
  } else {
    USB_LOG(EVENT) << "Failed to get device descriptor: "
                   << ConvertPlatformUsbErrorToString(rv);
  }
}

void UsbDeviceImpl::RefreshActiveConfiguration() {
  uint8_t config_value;
  int rv = libusb_get_active_config_value(platform_device_, &config_value);
  if (rv != LIBUSB_SUCCESS) {
    USB_LOG(EVENT) << "Failed to get active configuration: "
                   << ConvertPlatformUsbErrorToString(rv);
    return;
  }

  ActiveConfigurationChanged(config_value);
}

void UsbDeviceImpl::OpenOnBlockingThread(
    OpenCallback callback,
    scoped_refptr<base::TaskRunner> task_runner,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner) {
  base::ThreadRestrictions::AssertIOAllowed();
  PlatformUsbDeviceHandle handle;
  const int rv = libusb_open(platform_device_, &handle);
  if (LIBUSB_SUCCESS == rv) {
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(&UsbDeviceImpl::Opened, this, handle,
                                  std::move(callback), blocking_task_runner));
  } else {
    USB_LOG(EVENT) << "Failed to open device: "
                   << ConvertPlatformUsbErrorToString(rv);
    task_runner->PostTask(FROM_HERE,
                          base::BindOnce(std::move(callback), nullptr));
  }
}

void UsbDeviceImpl::Opened(
    PlatformUsbDeviceHandle platform_handle,
    OpenCallback callback,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner) {
  DCHECK(thread_checker_.CalledOnValidThread());
  scoped_refptr<UsbDeviceHandle> device_handle = new UsbDeviceHandleImpl(
      context_, this, platform_handle, blocking_task_runner);
  handles().push_back(device_handle.get());
  std::move(callback).Run(device_handle);
}

}  // namespace device

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_device.h"

#include <algorithm>

#include "base/stl_util.h"
#include "chrome/browser/usb/usb_context.h"
#include "chrome/browser/usb/usb_device_handle.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/libusb/src/libusb/libusb.h"

#if defined(OS_CHROMEOS)
#include "base/sys_info.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/permission_broker_client.h"
#endif  // defined(OS_CHROMEOS)

using content::BrowserThread;

namespace {

#if defined(OS_CHROMEOS)
void OnRequestUsbAccessReplied(
    const base::Callback<void(bool success)>& callback,
    bool success) {
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(callback, success));
}
#endif  // defined(OS_CHROMEOS)

}  // namespace

UsbDevice::UsbDevice(
    scoped_refptr<UsbContext> context,
    PlatformUsbDevice platform_device,
    uint16 vendor_id,
    uint16 product_id,
    uint32 unique_id)
    : platform_device_(platform_device),
      vendor_id_(vendor_id),
      product_id_(product_id),
      unique_id_(unique_id),
      context_(context) {
  CHECK(platform_device) << "platform_device cannot be NULL";
  libusb_ref_device(platform_device);
}

UsbDevice::UsbDevice()
    : platform_device_(NULL),
      vendor_id_(0),
      product_id_(0),
      unique_id_(0),
      context_(NULL) {
}

UsbDevice::~UsbDevice() {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (HandlesVector::iterator it = handles_.begin();
      it != handles_.end();
      ++it) {
    (*it)->InternalClose();
  }
  STLClearObject(&handles_);
  libusb_unref_device(platform_device_);
}

#if defined(OS_CHROMEOS)

void UsbDevice::RequestUsbAcess(
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

    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&chromeos::PermissionBrokerClient::RequestUsbAccess,
                   base::Unretained(client),
                   this->vendor_id_,
                   this->product_id_,
                   interface_id,
                   base::Bind(&OnRequestUsbAccessReplied, callback)));
  }
}

#endif

scoped_refptr<UsbDeviceHandle> UsbDevice::Open() {
  DCHECK(thread_checker_.CalledOnValidThread());
  PlatformUsbDeviceHandle handle;
  int rv = libusb_open(platform_device_, &handle);
  if (LIBUSB_SUCCESS == rv) {
    scoped_refptr<UsbConfigDescriptor> interfaces = ListInterfaces();
    if (!interfaces)
      return NULL;
    scoped_refptr<UsbDeviceHandle> device_handle =
        new UsbDeviceHandle(context_, this, handle, interfaces);
    handles_.push_back(device_handle);
    return device_handle;
  }
  return NULL;
}

bool UsbDevice::Close(scoped_refptr<UsbDeviceHandle> handle) {
  DCHECK(thread_checker_.CalledOnValidThread());

  for (HandlesVector::iterator it = handles_.begin();
      it != handles_.end();
      ++it) {
    if (*it == handle) {
      (*it)->InternalClose();
      handles_.erase(it);
      return true;
    }
  }
  return false;
}

scoped_refptr<UsbConfigDescriptor> UsbDevice::ListInterfaces() {
  DCHECK(thread_checker_.CalledOnValidThread());

  PlatformUsbConfigDescriptor platform_config;
  const int list_result =
      libusb_get_active_config_descriptor(platform_device_, &platform_config);
  if (list_result == 0)
    return new UsbConfigDescriptor(platform_config);

  return NULL;
}

void UsbDevice::OnDisconnect() {
  DCHECK(thread_checker_.CalledOnValidThread());
  HandlesVector handles;
  swap(handles, handles_);
  for (std::vector<scoped_refptr<UsbDeviceHandle> >::iterator it =
      handles.begin();
      it != handles.end();
      ++it) {
    (*it)->InternalClose();
  }
}

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_service.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/stl_util.h"
#include "chrome/browser/usb/usb_device.h"
#include "third_party/libusb/libusb.h"

UsbService::UsbService() : running_(true), thread_("UsbThread") {
  libusb_init(&context_);
  thread_.Start();
  PostHandleEventTask();
}

UsbService::~UsbService() {}

// TODO(gdk): There is currently no clean way to indicate to the event handler
// thread that it must break out of the handling loop before the event timeout,
// therefore we currently are at the whim of the event handler timeout before
// the message handling thread can be joined.
void UsbService::Cleanup() {
  running_ = false;

  if (!devices_.empty()) {
    libusb_close(devices_.begin()->second->handle());
  } else {
    LOG(WARNING) << "UsbService cannot force the USB event-handler thread to "
                 << "exit because there are no open devices with which to "
                 << "manipulate it. It maybe take up to 60 (!) seconds for the "
                 << "thread to join from this point.";
  }

  thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &UsbService::PlatformShutdown, base::Unretained(this)));
}

UsbDevice* UsbService::FindDevice(const uint16 vendor_id,
                                  const uint16 product_id) {
  const std::pair<uint16, uint16> key = std::make_pair(vendor_id, product_id);
  if (ContainsKey(devices_, key)) {
    return devices_[key];
  }

  libusb_device_handle* const handle = libusb_open_device_with_vid_pid(
      context_, vendor_id, product_id);
  if (!handle) {
    return NULL;
  }

  UsbDevice* const device = new UsbDevice(this, handle);
  devices_[key] = device;

  return device;
}

void UsbService::CloseDevice(scoped_refptr<UsbDevice> device) {
  DCHECK(running_) << "Cannot close device after service has stopped running.";

  for (DeviceMap::iterator i = devices_.begin(); i != devices_.end(); ++i) {
    if (i->second.get() == device.get()) {
      devices_.erase(i);
      libusb_close(device->handle());
      return;
    }
  }
}

void UsbService::PostHandleEventTask() {
  thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &UsbService::HandleEvent, base::Unretained(this)));
}

void UsbService::HandleEvent() {
  // TODO(gdk): Once there is a reasonable expectation that platforms will use
  // libusb >= 1.0.9 this should be changed to handle_events_completed, as the
  // use of handle_events is deprecated.
  libusb_handle_events(context_);
  if (running_) {
    PostHandleEventTask();
  }
}

void UsbService::PlatformShutdown() {
  libusb_exit(context_);
}

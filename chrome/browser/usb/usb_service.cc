// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_service.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/browser/usb/usb_device.h"
#include "third_party/libusb/libusb.h"

// The UsbEventHandler works around a design flaw in the libusb interface. There
// is currently no way to signal to libusb that any caller into one of the event
// handler calls should return without handling any events.
class UsbEventHandler : public base::PlatformThread::Delegate {
 public:
  explicit UsbEventHandler(PlatformUsbContext context)
      : running_(true), context_(context) {
    base::PlatformThread::CreateNonJoinable(0, this);
  }

  virtual ~UsbEventHandler() {}

  virtual void ThreadMain() {
    base::PlatformThread::SetName("UsbEventHandler");

    DLOG(INFO) << "UsbEventHandler started.";
    while (running_) {
      libusb_handle_events(context_);
    }
    DLOG(INFO) << "UsbEventHandler shutting down.";
    libusb_exit(context_);

    delete this;
  }

  void Stop() {
    running_ = false;
  }

 private:
  bool running_;
  PlatformUsbContext context_;

  DISALLOW_EVIL_CONSTRUCTORS(UsbEventHandler);
};

UsbService::UsbService() {
  libusb_init(&context_);
  event_handler_ = new UsbEventHandler(context_);
}

UsbService::~UsbService() {}

void UsbService::Cleanup() {
  event_handler_->Stop();
  event_handler_ = NULL;
}

UsbDevice* UsbService::FindDevice(const uint16 vendor_id,
                                  const uint16 product_id) {
  DCHECK(event_handler_) << "FindDevice called after event handler stopped.";

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
  DCHECK(event_handler_) << "CloseDevice called after event handler stopped.";

  for (DeviceMap::iterator i = devices_.begin(); i != devices_.end(); ++i) {
    if (i->second.get() == device.get()) {
      devices_.erase(i);
      libusb_close(device->handle());
      return;
    }
  }
}

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_context.h"

#include "base/atomicops.h"
#include "base/logging.h"
#include "base/threading/platform_thread.h"
#include "device/usb/usb_error.h"
#include "third_party/libusb/src/libusb/interrupt.h"
#include "third_party/libusb/src/libusb/libusb.h"

namespace device {

// The UsbEventHandler works around a design flaw in the libusb interface. There
// is currently no way to signal to libusb that any caller into one of the event
// handler calls should return without handling any events.
class UsbContext::UsbEventHandler : public base::PlatformThread::Delegate {
 public:
  explicit UsbEventHandler(libusb_context* context);
  ~UsbEventHandler() override;

  // base::PlatformThread::Delegate
  void ThreadMain() override;

  void Stop();

 private:
  base::subtle::Atomic32 running_;
  libusb_context* context_;
  base::PlatformThreadHandle thread_handle_;
  DISALLOW_COPY_AND_ASSIGN(UsbEventHandler);
};

UsbContext::UsbEventHandler::UsbEventHandler(libusb_context* context)
    : context_(context), thread_handle_(0) {
  base::subtle::Release_Store(&running_, 1);
  bool success = base::PlatformThread::Create(0, this, &thread_handle_);
  DCHECK(success) << "Failed to create USB IO handling thread.";
}

UsbContext::UsbEventHandler::~UsbEventHandler() {
  libusb_exit(context_);
}

void UsbContext::UsbEventHandler::ThreadMain() {
  base::PlatformThread::SetName("UsbEventHandler");
  VLOG(1) << "UsbEventHandler started.";

  while (base::subtle::Acquire_Load(&running_)) {
    const int rv = libusb_handle_events(context_);
    if (rv != LIBUSB_SUCCESS) {
      VLOG(1) << "Failed to handle events: "
              << ConvertPlatformUsbErrorToString(rv);
    }
  }

  VLOG(1) << "UsbEventHandler shutting down.";
  delete this;
}

void UsbContext::UsbEventHandler::Stop() {
  base::PlatformThreadHandle thread_handle = thread_handle_;
  base::subtle::Release_Store(&running_, 0);
  libusb_interrupt_handle_event(context_);
  base::PlatformThread::Join(thread_handle);
}

UsbContext::UsbContext(PlatformUsbContext context) : context_(context) {
  // Ownership of the PlatformUsbContext is passed to the event handler thread.
  event_handler_ = new UsbEventHandler(context_);
}

UsbContext::~UsbContext() {
  DCHECK(thread_checker_.CalledOnValidThread());
  event_handler_->Stop();
}

}  // namespace device

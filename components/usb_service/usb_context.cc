// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/usb_service/usb_context.h"

#include "base/atomicops.h"
#include "base/logging.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "components/usb_service/usb_error.h"
#include "third_party/libusb/src/libusb/interrupt.h"
#include "third_party/libusb/src/libusb/libusb.h"

namespace usb_service {

// The UsbEventHandler works around a design flaw in the libusb interface. There
// is currently no way to signal to libusb that any caller into one of the event
// handler calls should return without handling any events.
class UsbContext::UsbEventHandler : public base::PlatformThread::Delegate {
 public:
  explicit UsbEventHandler(libusb_context* context);
  virtual ~UsbEventHandler();

  // base::PlatformThread::Delegate
  virtual void ThreadMain() OVERRIDE;

 private:
  base::subtle::Atomic32 running_;
  libusb_context* context_;
  base::PlatformThreadHandle thread_handle_;
  base::WaitableEvent start_polling_;
  DISALLOW_COPY_AND_ASSIGN(UsbEventHandler);
};

UsbContext::UsbEventHandler::UsbEventHandler(libusb_context* context)
    : context_(context),
      thread_handle_(0),
      start_polling_(false, false) {
  base::subtle::Release_Store(&running_, 1);
  bool success = base::PlatformThread::Create(0, this, &thread_handle_);
  DCHECK(success) << "Failed to create USB IO handling thread.";
  start_polling_.Wait();
}

UsbContext::UsbEventHandler::~UsbEventHandler() {
  base::subtle::Release_Store(&running_, 0);
  libusb_interrupt_handle_event(context_);
  base::PlatformThread::Join(thread_handle_);
}

void UsbContext::UsbEventHandler::ThreadMain() {
  base::PlatformThread::SetName("UsbEventHandler");
  VLOG(1) << "UsbEventHandler started.";

  if (base::subtle::Acquire_Load(&running_)) {
    start_polling_.Signal();
  }
  while (base::subtle::Acquire_Load(&running_)) {
    const int rv = libusb_handle_events(context_);
    if (rv != LIBUSB_SUCCESS) {
      VLOG(1) << "Failed to handle events: " << ConvertErrorToString(rv);
    }
  }
  VLOG(1) << "UsbEventHandler shutting down.";
}

UsbContext::UsbContext(PlatformUsbContext context) : context_(context) {
  DCHECK(thread_checker_.CalledOnValidThread());
  event_handler_ = new UsbEventHandler(context_);
}

UsbContext::~UsbContext() {
  // destruction of UsbEventHandler is a blocking operation.
  DCHECK(thread_checker_.CalledOnValidThread());
  delete event_handler_;
  event_handler_ = NULL;
  libusb_exit(context_);
}

}  // namespace usb_service

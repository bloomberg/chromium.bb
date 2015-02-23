// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_service.h"

#include "base/message_loop/message_loop.h"
#include "components/device_event_log/device_event_log.h"
#include "device/usb/usb_device.h"
#include "device/usb/usb_service_impl.h"

namespace device {

namespace {

UsbService* g_service;

}  // namespace

// This class manages the lifetime of the global UsbService instance so that
// it is destroyed when the current message loop is destroyed. A lazy instance
// cannot be used because this object does not live on the main thread.
class UsbService::Destroyer : private base::MessageLoop::DestructionObserver {
 public:
  explicit Destroyer(UsbService* usb_service) : usb_service_(usb_service) {
    base::MessageLoop::current()->AddDestructionObserver(this);
  }
  ~Destroyer() override {}

 private:
  // base::MessageLoop::DestructionObserver implementation.
  void WillDestroyCurrentMessageLoop() override {
    base::MessageLoop::current()->RemoveDestructionObserver(this);
    delete usb_service_;
    delete this;
    g_service = nullptr;
  }

  UsbService* usb_service_;
};

void UsbService::Observer::OnDeviceAdded(scoped_refptr<UsbDevice> device) {
}

void UsbService::Observer::OnDeviceRemoved(scoped_refptr<UsbDevice> device) {
}

void UsbService::Observer::OnDeviceRemovedCleanup(
    scoped_refptr<UsbDevice> device) {
}

// static
UsbService* UsbService::GetInstance(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  if (!g_service) {
    g_service = UsbServiceImpl::Create(ui_task_runner);
    // This object will clean itself up when the message loop is destroyed.
    new Destroyer(g_service);
  }
  return g_service;
}

// static
void UsbService::SetInstanceForTest(UsbService* instance) {
  g_service = instance;
  new Destroyer(instance);
}

UsbService::UsbService() {
}

UsbService::~UsbService() {
}

void UsbService::AddObserver(Observer* observer) {
  DCHECK(CalledOnValidThread());
  observer_list_.AddObserver(observer);
}

void UsbService::RemoveObserver(Observer* observer) {
  DCHECK(CalledOnValidThread());
  observer_list_.RemoveObserver(observer);
}

void UsbService::NotifyDeviceAdded(scoped_refptr<UsbDevice> device) {
  DCHECK(CalledOnValidThread());

  USB_LOG(USER) << "USB device added: vendorId = " << device->vendor_id()
                << ", productId = " << device->product_id()
                << ", uniqueId = " << device->unique_id();

  FOR_EACH_OBSERVER(Observer, observer_list_, OnDeviceAdded(device));
}

void UsbService::NotifyDeviceRemoved(scoped_refptr<UsbDevice> device) {
  DCHECK(CalledOnValidThread());

  USB_LOG(USER) << "USB device removed: uniqueId = " << device->unique_id();

  FOR_EACH_OBSERVER(Observer, observer_list_, OnDeviceRemoved(device));
  FOR_EACH_OBSERVER(Observer, observer_list_, OnDeviceRemovedCleanup(device));
}

}  // namespace device

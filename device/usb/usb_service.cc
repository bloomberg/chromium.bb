// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_service.h"

#include "base/at_exit.h"
#include "base/bind.h"
#include "build/build_config.h"
#include "components/device_event_log/device_event_log.h"
#include "device/usb/usb_device.h"

#if defined(OS_ANDROID)
#include "device/usb/usb_service_android.h"
#else
#include "device/usb/usb_service_impl.h"
#endif

namespace device {

UsbService::Observer::~Observer() {}

void UsbService::Observer::OnDeviceAdded(scoped_refptr<UsbDevice> device) {
}

void UsbService::Observer::OnDeviceRemoved(scoped_refptr<UsbDevice> device) {
}

void UsbService::Observer::OnDeviceRemovedCleanup(
    scoped_refptr<UsbDevice> device) {
}

void UsbService::Observer::WillDestroyUsbService() {}

// static
scoped_ptr<UsbService> UsbService::Create(
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner) {
#if defined(OS_ANDROID)
  return make_scoped_ptr(new UsbServiceAndroid());
#else
  return make_scoped_ptr(new UsbServiceImpl(blocking_task_runner));
#endif
}

UsbService::~UsbService() {
  FOR_EACH_OBSERVER(Observer, observer_list_, WillDestroyUsbService());
}

UsbService::UsbService() {}

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

  FOR_EACH_OBSERVER(Observer, observer_list_, OnDeviceAdded(device));
}

void UsbService::NotifyDeviceRemoved(scoped_refptr<UsbDevice> device) {
  DCHECK(CalledOnValidThread());

  FOR_EACH_OBSERVER(Observer, observer_list_, OnDeviceRemoved(device));
  FOR_EACH_OBSERVER(Observer, observer_list_, OnDeviceRemovedCleanup(device));
}

}  // namespace device

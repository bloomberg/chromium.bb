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

void UsbService::Observer::OnDeviceAdded(scoped_refptr<UsbDevice> device) {
}

void UsbService::Observer::OnDeviceRemoved(scoped_refptr<UsbDevice> device) {
}

void UsbService::Observer::OnDeviceRemovedCleanup(
    scoped_refptr<UsbDevice> device) {
}

// static
UsbService* UsbService::GetInstance(
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner) {
  if (!g_service) {
    // The UsbService constructor saves this object and UsbServiceImpl will
    // destroy itself when the current message loop exits.
    new UsbServiceImpl(blocking_task_runner);
  }
  return g_service;
}

UsbService::UsbService() {
  DCHECK(!g_service);
  g_service = this;
}

UsbService::~UsbService() {
  DCHECK(g_service);
  g_service = nullptr;
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

  FOR_EACH_OBSERVER(Observer, observer_list_, OnDeviceAdded(device));
}

void UsbService::NotifyDeviceRemoved(scoped_refptr<UsbDevice> device) {
  DCHECK(CalledOnValidThread());

  FOR_EACH_OBSERVER(Observer, observer_list_, OnDeviceRemoved(device));
  FOR_EACH_OBSERVER(Observer, observer_list_, OnDeviceRemovedCleanup(device));
}

}  // namespace device

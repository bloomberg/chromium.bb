// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/usb/usb_api.h"

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/usb/usb_device_resource.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/usb/usb_service.h"
#include "chrome/browser/usb/usb_service_factory.h"
#include "chrome/common/extensions/api/experimental_usb.h"

namespace BulkTransfer = extensions::api::experimental_usb::BulkTransfer;
namespace CloseDevice = extensions::api::experimental_usb::CloseDevice;
namespace ControlTransfer = extensions::api::experimental_usb::ControlTransfer;
namespace FindDevice = extensions::api::experimental_usb::FindDevice;
namespace InterruptTransfer =
    extensions::api::experimental_usb::InterruptTransfer;
namespace IsochronousTransfer =
    extensions::api::experimental_usb::IsochronousTransfer;
using extensions::api::experimental_usb::Device;
using std::vector;

namespace {

static UsbDevice* device_for_test_ = NULL;

}  // namespace

namespace extensions {

UsbAsyncApiFunction::UsbAsyncApiFunction()
    : manager_(NULL) {
}

UsbAsyncApiFunction::~UsbAsyncApiFunction() {
}

bool UsbAsyncApiFunction::PrePrepare() {
  manager_ = ExtensionSystem::Get(profile())->usb_device_resource_manager();
  return manager_ != NULL;
}

UsbDeviceResource* UsbAsyncApiFunction::GetUsbDeviceResource(
    int api_resource_id) {
  return manager_->Get(extension_->id(), api_resource_id);
}

void UsbAsyncApiFunction::RemoveUsbDeviceResource(int api_resource_id) {
  manager_->Remove(extension_->id(), api_resource_id);
}

UsbFindDeviceFunction::UsbFindDeviceFunction() : event_notifier_(NULL) {}

UsbFindDeviceFunction::~UsbFindDeviceFunction() {}

void UsbFindDeviceFunction::SetDeviceForTest(UsbDevice* device) {
  device_for_test_ = device;
}

bool UsbFindDeviceFunction::Prepare() {
  parameters_ = FindDevice::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  event_notifier_ = CreateEventNotifier(DeprecatedExtractSrcId(2));
  return true;
}

void UsbFindDeviceFunction::Work() {
  UsbDevice* device = NULL;
  if (device_for_test_) {
    device = device_for_test_;
  } else {
    UsbService* const service = UsbServiceFactory::GetInstance()->GetForProfile(
        profile());
    device = service->FindDevice(parameters_->vendor_id,
                                 parameters_->product_id);
  }

  if (!device) {
    SetResult(base::Value::CreateNullValue());
    return;
  }

  UsbDeviceResource* const resource = new UsbDeviceResource(extension_->id(),
                                                            event_notifier_,
                                                            device);

  Device result;
  result.handle = manager_->Add(resource);
  result.vendor_id = parameters_->vendor_id;
  result.product_id = parameters_->product_id;
  results_ = FindDevice::Results::Create(result);
}

bool UsbFindDeviceFunction::Respond() {
  return true;
}

UsbCloseDeviceFunction::UsbCloseDeviceFunction() {}

UsbCloseDeviceFunction::~UsbCloseDeviceFunction() {}

bool UsbCloseDeviceFunction::Prepare() {
  parameters_ = CloseDevice::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbCloseDeviceFunction::Work() {
  UsbDeviceResource* const device = GetUsbDeviceResource(
      parameters_->device.handle);

  if (device)
    device->Close();

  RemoveUsbDeviceResource(parameters_->device.handle);
}

bool UsbCloseDeviceFunction::Respond() {
  return true;
}

UsbControlTransferFunction::UsbControlTransferFunction() {}

UsbControlTransferFunction::~UsbControlTransferFunction() {}

bool UsbControlTransferFunction::Prepare() {
  parameters_ = ControlTransfer::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbControlTransferFunction::Work() {
  UsbDeviceResource* const device = GetUsbDeviceResource(
      parameters_->device.handle);

  if (device) {
    device->ControlTransfer(parameters_->transfer_info);
  }
}

bool UsbControlTransferFunction::Respond() {
  return true;
}

bool UsbBulkTransferFunction::Prepare() {
  parameters_ = BulkTransfer::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

UsbBulkTransferFunction::UsbBulkTransferFunction() {}

UsbBulkTransferFunction::~UsbBulkTransferFunction() {}

void UsbBulkTransferFunction::Work() {
  UsbDeviceResource* const device = GetUsbDeviceResource(
      parameters_->device.handle);

  if (device) {
    device->BulkTransfer(parameters_->transfer_info);
  }
}

bool UsbBulkTransferFunction::Respond() {
  return true;
}

UsbInterruptTransferFunction::UsbInterruptTransferFunction() {}

UsbInterruptTransferFunction::~UsbInterruptTransferFunction() {}

bool UsbInterruptTransferFunction::Prepare() {
  parameters_ = InterruptTransfer::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbInterruptTransferFunction::Work() {
  UsbDeviceResource* const device = GetUsbDeviceResource(
      parameters_->device.handle);

  if (device) {
    device->InterruptTransfer(parameters_->transfer_info);
  }
}

bool UsbInterruptTransferFunction::Respond() {
  return true;
}

UsbIsochronousTransferFunction::UsbIsochronousTransferFunction() {}

UsbIsochronousTransferFunction::~UsbIsochronousTransferFunction() {}

bool UsbIsochronousTransferFunction::Prepare() {
  parameters_ = IsochronousTransfer::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbIsochronousTransferFunction::Work() {
  UsbDeviceResource* const device = GetUsbDeviceResource(
      parameters_->device.handle);

  if (device) {
    device->IsochronousTransfer(parameters_->transfer_info);
  }
}

bool UsbIsochronousTransferFunction::Respond() {
  return true;
}

}  // namespace extensions

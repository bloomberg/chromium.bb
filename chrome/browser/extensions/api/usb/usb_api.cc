// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/usb/usb_api.h"

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/api_resource_controller.h"
#include "chrome/browser/extensions/api/usb/usb_device_resource.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/usb/usb_service_factory.h"
#include "chrome/browser/usb/usb_service.h"
#include "chrome/common/extensions/api/experimental_usb.h"

namespace BulkTransfer = extensions::api::experimental_usb::BulkTransfer;
namespace CloseDevice = extensions::api::experimental_usb::CloseDevice;
namespace ControlTransfer = extensions::api::experimental_usb::ControlTransfer;
namespace FindDevice = extensions::api::experimental_usb::FindDevice;
namespace InterruptTransfer =
    extensions::api::experimental_usb::InterruptTransfer;
using extensions::api::experimental_usb::Device;
using std::vector;

namespace extensions {

UsbFindDeviceFunction::UsbFindDeviceFunction() : event_notifier_(NULL) {}

UsbFindDeviceFunction::~UsbFindDeviceFunction() {}

bool UsbFindDeviceFunction::Prepare() {
  parameters_ = FindDevice::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  event_notifier_ = CreateEventNotifier(ExtractSrcId(2));
  return true;
}

void UsbFindDeviceFunction::Work() {
  UsbService* const service =
      UsbServiceFactory::GetInstance()->GetForProfile(profile());
  DCHECK(service) << "No UsbService associated with profile.";

  UsbDevice* const device = service->FindDevice(parameters_->vendor_id,
                                                parameters_->product_id);
  if (!device) {
    result_.reset(base::Value::CreateNullValue());
    return;
  }

  UsbDeviceResource* const resource = new UsbDeviceResource(event_notifier_,
                                                            device);

  Device result;
  result.handle = controller()->AddAPIResource(resource);
  result.vendor_id = parameters_->vendor_id;
  result.product_id = parameters_->product_id;
  result_ = result.ToValue();
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
  controller()->RemoveAPIResource(parameters_->device.handle);
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
  UsbDeviceResource* const device = controller()->GetUsbDeviceResource(
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
  UsbDeviceResource* const device = controller()->GetUsbDeviceResource(
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
  UsbDeviceResource* const device = controller()->GetUsbDeviceResource(
      parameters_->device.handle);
  if (device) {
    device->InterruptTransfer(parameters_->transfer_info);
  }
}

bool UsbInterruptTransferFunction::Respond() {
  return true;
}

}  // namespace extensions

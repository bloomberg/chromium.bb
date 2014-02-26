// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/hid/hid_api.h"

#include <string>
#include <vector>

#include "chrome/browser/extensions/api/api_resource_manager.h"
#include "chrome/browser/extensions/api/hid/hid_device_resource.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/hid.h"
#include "chrome/common/extensions/permissions/usb_device_permission.h"
#include "device/hid/hid_connection.h"
#include "device/hid/hid_device_info.h"
#include "device/hid/hid_service.h"
#include "extensions/common/permissions/permissions_data.h"
#include "net/base/io_buffer.h"

namespace hid = extensions::api::hid;

using device::HidConnection;
using device::HidService;
using device::HidDeviceInfo;

namespace {

const char kErrorPermissionDenied[] = "Permission to access device was denied.";
const char kErrorServiceFailed[] = "HID service failed be created.";
const char kErrorDeviceNotFound[] = "HID device not found.";
const char kErrorFailedToOpenDevice[] = "Failed to open HID device.";
const char kErrorConnectionNotFound[] = "Connection not established.";
const char kErrorTransfer[] = "Transfer failed.";

base::Value* PopulateHidDevice(const HidDeviceInfo& info) {
  hid::HidDeviceInfo device_info;
  device_info.path = info.device_id;
  device_info.product_id = info.product_id;
  device_info.vendor_id = info.vendor_id;
  return device_info.ToValue().release();
}

base::Value* PopulateHidConnection(int connection_id,
                                   scoped_refptr<HidConnection> connection) {
  hid::HidConnectInfo connection_value;
  connection_value.connection_id = connection_id;
  return connection_value.ToValue().release();
}

}  // namespace

namespace extensions {

HidAsyncApiFunction::HidAsyncApiFunction() : manager_(NULL) {
}

HidAsyncApiFunction::~HidAsyncApiFunction() {}

bool HidAsyncApiFunction::PrePrepare() {
  manager_ = ApiResourceManager<HidConnectionResource>::Get(GetProfile());
  if (!manager_) return false;
  set_work_thread_id(content::BrowserThread::FILE);
  return true;
}

bool HidAsyncApiFunction::Respond() { return error_.empty(); }

HidConnectionResource* HidAsyncApiFunction::GetHidConnectionResource(
    int api_resource_id) {
  return manager_->Get(extension_->id(), api_resource_id);
}

void HidAsyncApiFunction::RemoveHidConnectionResource(int api_resource_id) {
  manager_->Remove(extension_->id(), api_resource_id);
}

void HidAsyncApiFunction::CompleteWithError(const std::string& error) {
  SetError(error);
  AsyncWorkCompleted();
}

HidGetDevicesFunction::HidGetDevicesFunction() {}

HidGetDevicesFunction::~HidGetDevicesFunction() {}

bool HidGetDevicesFunction::Prepare() {
  parameters_ = hid::GetDevices::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void HidGetDevicesFunction::AsyncWorkStart() {
  const uint16_t vendor_id = parameters_->options.vendor_id;
  const uint16_t product_id = parameters_->options.product_id;
  UsbDevicePermission::CheckParam param(
      vendor_id, product_id, UsbDevicePermissionData::UNSPECIFIED_INTERFACE);
  if (!PermissionsData::CheckAPIPermissionWithParam(
          GetExtension(), APIPermission::kUsbDevice, &param)) {
    LOG(WARNING) << "Insufficient permissions to access device.";
    CompleteWithError(kErrorPermissionDenied);
    return;
  }

  HidService* service = HidService::GetInstance();
  if (!service) {
    CompleteWithError(kErrorServiceFailed);
    return;
  }
  std::vector<HidDeviceInfo> devices;
  service->GetDevices(&devices);

  scoped_ptr<base::ListValue> result(new base::ListValue());
  for (std::vector<HidDeviceInfo>::iterator it = devices.begin();
       it != devices.end(); it++) {
    if (it->product_id == product_id &&
        it->vendor_id == vendor_id)
      result->Append(PopulateHidDevice(*it));
  }
  SetResult(result.release());
  AsyncWorkCompleted();
}

HidConnectFunction::HidConnectFunction() {}

HidConnectFunction::~HidConnectFunction() {}

bool HidConnectFunction::Prepare() {
  parameters_ = hid::Connect::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void HidConnectFunction::AsyncWorkStart() {
  HidService* service = HidService::GetInstance();
  if (!service) {
    CompleteWithError(kErrorServiceFailed);
    return;
  }
  HidDeviceInfo device;
  if (!service->GetInfo(parameters_->device_info.path, &device)) {
    CompleteWithError(kErrorDeviceNotFound);
    return;
  }
  if (device.vendor_id != parameters_->device_info.vendor_id ||
      device.product_id != parameters_->device_info.product_id) {
    CompleteWithError(kErrorDeviceNotFound);
    return;
  }
  scoped_refptr<HidConnection> connection = service->Connect(device.device_id);
  if (!connection) {
    CompleteWithError(kErrorFailedToOpenDevice);
    return;
  }
  int connection_id =
      manager_->Add(new HidConnectionResource(extension_->id(), connection));
  SetResult(PopulateHidConnection(connection_id, connection));
  AsyncWorkCompleted();
}

HidDisconnectFunction::HidDisconnectFunction() {}

HidDisconnectFunction::~HidDisconnectFunction() {}

bool HidDisconnectFunction::Prepare() {
  parameters_ = hid::Disconnect::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void HidDisconnectFunction::AsyncWorkStart() {
  int connection_id = parameters_->connection_id;
  HidConnectionResource* resource =
      manager_->Get(extension_->id(), connection_id);
  if (!resource) {
    CompleteWithError(kErrorConnectionNotFound);
    return;
  }
  manager_->Remove(extension_->id(), connection_id);
  AsyncWorkCompleted();
}

HidReceiveFunction::HidReceiveFunction() {}

HidReceiveFunction::~HidReceiveFunction() {}

bool HidReceiveFunction::Prepare() {
  parameters_ = hid::Receive::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void HidReceiveFunction::AsyncWorkStart() {
  int connection_id = parameters_->connection_id;
  HidConnectionResource* resource =
      manager_->Get(extension_->id(), connection_id);
  if (!resource) {
    CompleteWithError(kErrorConnectionNotFound);
    return;
  }

  buffer_ = new net::IOBuffer(parameters_->size);
  resource->connection()->Read(
      buffer_,
      parameters_->size,
      base::Bind(&HidReceiveFunction::OnFinished, this));
}

void HidReceiveFunction::OnFinished(bool success, size_t bytes) {
  if (!success) {
    CompleteWithError(kErrorTransfer);
    return;
  }

  SetResult(base::BinaryValue::CreateWithCopiedBuffer(buffer_->data(), bytes));
  AsyncWorkCompleted();
}

HidSendFunction::HidSendFunction() {}

HidSendFunction::~HidSendFunction() {}

bool HidSendFunction::Prepare() {
  parameters_ = hid::Send::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void HidSendFunction::AsyncWorkStart() {
  int connection_id = parameters_->connection_id;
  HidConnectionResource* resource =
      manager_->Get(extension_->id(), connection_id);
  if (!resource) {
    CompleteWithError(kErrorConnectionNotFound);
    return;
  }

  scoped_refptr<net::IOBuffer> buffer(
      new net::WrappedIOBuffer(parameters_->data.c_str()));
  memcpy(buffer->data(),
         parameters_->data.c_str(),
         parameters_->data.size());
  resource->connection()->Write(
      buffer,
      parameters_->data.size(),
      base::Bind(&HidSendFunction::OnFinished, this));
}

void HidSendFunction::OnFinished(bool success, size_t bytes) {
  if (!success) {
    CompleteWithError(kErrorTransfer);
    return;
  }
  AsyncWorkCompleted();
}

HidReceiveFeatureReportFunction::HidReceiveFeatureReportFunction() {}

HidReceiveFeatureReportFunction::~HidReceiveFeatureReportFunction() {}

bool HidReceiveFeatureReportFunction::Prepare() {
  parameters_ = hid::ReceiveFeatureReport::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void HidReceiveFeatureReportFunction::AsyncWorkStart() {
  int connection_id = parameters_->connection_id;
  HidConnectionResource* resource =
      manager_->Get(extension_->id(), connection_id);
  if (!resource) {
    CompleteWithError(kErrorConnectionNotFound);
    return;
  }
  buffer_ = new net::IOBuffer(parameters_->size);
  resource->connection()->GetFeatureReport(
      buffer_,
      parameters_->size,
      base::Bind(&HidReceiveFeatureReportFunction::OnFinished, this));
}

void HidReceiveFeatureReportFunction::OnFinished(bool success, size_t bytes) {
  if (!success) {
    CompleteWithError(kErrorTransfer);
    return;
  }

  SetResult(base::BinaryValue::CreateWithCopiedBuffer(buffer_->data(), bytes));
  AsyncWorkCompleted();
}

HidSendFeatureReportFunction::HidSendFeatureReportFunction() {}

HidSendFeatureReportFunction::~HidSendFeatureReportFunction() {}

bool HidSendFeatureReportFunction::Prepare() {
  parameters_ = hid::SendFeatureReport::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void HidSendFeatureReportFunction::AsyncWorkStart() {
  int connection_id = parameters_->connection_id;
  HidConnectionResource* resource =
      manager_->Get(extension_->id(), connection_id);
  if (!resource) {
    CompleteWithError(kErrorConnectionNotFound);
    return;
  }
  scoped_refptr<net::IOBuffer> buffer(
      new net::WrappedIOBuffer(parameters_->data.c_str()));
  resource->connection()->SendFeatureReport(
      buffer,
      parameters_->data.size(),
      base::Bind(&HidSendFeatureReportFunction::OnFinished, this));
}

void HidSendFeatureReportFunction::OnFinished(bool success, size_t bytes) {
  if (!success) {
    CompleteWithError(kErrorTransfer);
    return;
  }
  AsyncWorkCompleted();
}

}  // namespace extensions

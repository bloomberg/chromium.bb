// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/hid/hid_api.h"

#include <string>
#include <vector>

#include "chrome/common/extensions/api/hid.h"
#include "device/hid/hid_connection.h"
#include "device/hid/hid_device_info.h"
#include "device/hid/hid_service.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/usb_device_permission.h"
#include "net/base/io_buffer.h"

namespace hid = extensions::api::hid;

using device::HidConnection;
using device::HidDeviceInfo;
using device::HidService;

namespace {

const char kErrorPermissionDenied[] = "Permission to access device was denied.";
const char kErrorInvalidDeviceId[] = "Invalid HID device ID.";
const char kErrorFailedToOpenDevice[] = "Failed to open HID device.";
const char kErrorConnectionNotFound[] = "Connection not established.";
const char kErrorTransfer[] = "Transfer failed.";

base::Value* PopulateHidConnection(int connection_id,
                                   scoped_refptr<HidConnection> connection) {
  hid::HidConnectInfo connection_value;
  connection_value.connection_id = connection_id;
  return connection_value.ToValue().release();
}

}  // namespace

namespace extensions {

HidAsyncApiFunction::HidAsyncApiFunction()
    : device_manager_(NULL), connection_manager_(NULL) {}

HidAsyncApiFunction::~HidAsyncApiFunction() {}

bool HidAsyncApiFunction::PrePrepare() {
  device_manager_ = HidDeviceManager::Get(browser_context());
  DCHECK(device_manager_);
  connection_manager_ =
      ApiResourceManager<HidConnectionResource>::Get(browser_context());
  DCHECK(connection_manager_);
  set_work_thread_id(content::BrowserThread::FILE);
  return true;
}

bool HidAsyncApiFunction::Respond() { return error_.empty(); }

HidConnectionResource* HidAsyncApiFunction::GetHidConnectionResource(
    int api_resource_id) {
  return connection_manager_->Get(extension_->id(), api_resource_id);
}

void HidAsyncApiFunction::RemoveHidConnectionResource(int api_resource_id) {
  connection_manager_->Remove(extension_->id(), api_resource_id);
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
  if (!GetExtension()->permissions_data()->CheckAPIPermissionWithParam(
          APIPermission::kUsbDevice, &param)) {
    LOG(WARNING) << "Insufficient permissions to access device.";
    CompleteWithError(kErrorPermissionDenied);
    return;
  }

  SetResult(device_manager_->GetApiDevices(vendor_id, product_id).release());
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
  device::HidDeviceInfo device_info;
  if (!device_manager_->GetDeviceInfo(parameters_->device_id, &device_info)) {
    CompleteWithError(kErrorInvalidDeviceId);
    return;
  }

  UsbDevicePermission::CheckParam param(
      device_info.vendor_id,
      device_info.product_id,
      UsbDevicePermissionData::UNSPECIFIED_INTERFACE);
  if (!GetExtension()->permissions_data()->CheckAPIPermissionWithParam(
          APIPermission::kUsbDevice, &param)) {
    LOG(WARNING) << "Insufficient permissions to access device.";
    CompleteWithError(kErrorPermissionDenied);
    return;
  }

  HidService* hid_service = HidService::GetInstance();
  DCHECK(hid_service);
  scoped_refptr<HidConnection> connection =
      hid_service->Connect(device_info.device_id);
  if (!connection) {
    CompleteWithError(kErrorFailedToOpenDevice);
    return;
  }
  int connection_id = connection_manager_->Add(
      new HidConnectionResource(extension_->id(), connection));
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
      connection_manager_->Get(extension_->id(), connection_id);
  if (!resource) {
    CompleteWithError(kErrorConnectionNotFound);
    return;
  }
  connection_manager_->Remove(extension_->id(), connection_id);
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
      connection_manager_->Get(extension_->id(), connection_id);
  if (!resource) {
    CompleteWithError(kErrorConnectionNotFound);
    return;
  }

  buffer_ = new net::IOBufferWithSize(parameters_->size);
  resource->connection()->Read(
      buffer_, base::Bind(&HidReceiveFunction::OnFinished, this));
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
      connection_manager_->Get(extension_->id(), connection_id);
  if (!resource) {
    CompleteWithError(kErrorConnectionNotFound);
    return;
  }

  scoped_refptr<net::IOBufferWithSize> buffer(
      new net::IOBufferWithSize(parameters_->data.size()));
  memcpy(buffer->data(), parameters_->data.c_str(), parameters_->data.size());
  resource->connection()->Write(static_cast<uint8_t>(parameters_->report_id),
                                buffer,
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
      connection_manager_->Get(extension_->id(), connection_id);
  if (!resource) {
    CompleteWithError(kErrorConnectionNotFound);
    return;
  }
  buffer_ = new net::IOBufferWithSize(parameters_->size);
  resource->connection()->GetFeatureReport(
      static_cast<uint8_t>(parameters_->report_id),
      buffer_,
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
      connection_manager_->Get(extension_->id(), connection_id);
  if (!resource) {
    CompleteWithError(kErrorConnectionNotFound);
    return;
  }
  scoped_refptr<net::IOBufferWithSize> buffer(
      new net::IOBufferWithSize(parameters_->data.size()));
  resource->connection()->SendFeatureReport(
      static_cast<uint8_t>(parameters_->report_id),
      buffer,
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

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/hid/hid_api.h"

#include <string>
#include <vector>

#include "device/core/device_client.h"
#include "device/hid/hid_connection.h"
#include "device/hid/hid_device_filter.h"
#include "device/hid/hid_device_info.h"
#include "device/hid/hid_service.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/common/api/hid.h"
#include "net/base/io_buffer.h"

namespace hid = extensions::core_api::hid;

using device::HidConnection;
using device::HidDeviceFilter;
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

void ConvertHidDeviceFilter(linked_ptr<hid::DeviceFilter> input,
                            HidDeviceFilter* output) {
  if (input->vendor_id) {
    output->SetVendorId(*input->vendor_id);
  }
  if (input->product_id) {
    output->SetProductId(*input->product_id);
  }
  if (input->usage_page) {
    output->SetUsagePage(*input->usage_page);
  }
  if (input->usage) {
    output->SetUsage(*input->usage);
  }
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
  std::vector<HidDeviceFilter> filters;
  if (parameters_->options.filters) {
    filters.resize(parameters_->options.filters->size());
    for (size_t i = 0; i < parameters_->options.filters->size(); ++i) {
      ConvertHidDeviceFilter(parameters_->options.filters->at(i), &filters[i]);
    }
  }
  if (parameters_->options.vendor_id) {
    HidDeviceFilter legacy_filter;
    legacy_filter.SetVendorId(*parameters_->options.vendor_id);
    if (parameters_->options.product_id) {
      legacy_filter.SetProductId(*parameters_->options.product_id);
    }
    filters.push_back(legacy_filter);
  }

  SetResult(device_manager_->GetApiDevices(extension(), filters).release());
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

  if (!device_manager_->HasPermission(extension(), device_info)) {
    LOG(WARNING) << "Insufficient permissions to access device.";
    CompleteWithError(kErrorPermissionDenied);
    return;
  }

  HidService* hid_service = device::DeviceClient::Get()->GetHidService();
  DCHECK(hid_service);
  scoped_refptr<HidConnection> connection =
      hid_service->Connect(device_info.device_id);
  if (!connection.get()) {
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

  scoped_refptr<device::HidConnection> connection = resource->connection();
  connection->Read(base::Bind(&HidReceiveFunction::OnFinished, this));
}

void HidReceiveFunction::OnFinished(bool success,
                                    scoped_refptr<net::IOBuffer> buffer,
                                    size_t size) {
  if (!success) {
    CompleteWithError(kErrorTransfer);
    return;
  }

  DCHECK_GE(size, 1u);
  int report_id = reinterpret_cast<uint8_t*>(buffer->data())[0];

  scoped_ptr<base::ListValue> result(new base::ListValue());
  result->Append(new base::FundamentalValue(report_id));
  result->Append(
      base::BinaryValue::CreateWithCopiedBuffer(buffer->data() + 1, size - 1));
  SetResultList(result.Pass());
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
      new net::IOBufferWithSize(parameters_->data.size() + 1));
  buffer->data()[0] = static_cast<uint8_t>(parameters_->report_id);
  memcpy(
      buffer->data() + 1, parameters_->data.c_str(), parameters_->data.size());
  resource->connection()->Write(
      buffer, buffer->size(), base::Bind(&HidSendFunction::OnFinished, this));
}

void HidSendFunction::OnFinished(bool success) {
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

  scoped_refptr<device::HidConnection> connection = resource->connection();
  connection->GetFeatureReport(
      static_cast<uint8_t>(parameters_->report_id),
      base::Bind(&HidReceiveFeatureReportFunction::OnFinished, this));
}

void HidReceiveFeatureReportFunction::OnFinished(
    bool success,
    scoped_refptr<net::IOBuffer> buffer,
    size_t size) {
  if (!success) {
    CompleteWithError(kErrorTransfer);
    return;
  }

  SetResult(base::BinaryValue::CreateWithCopiedBuffer(buffer->data(), size));
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
      new net::IOBufferWithSize(parameters_->data.size() + 1));
  buffer->data()[0] = static_cast<uint8_t>(parameters_->report_id);
  memcpy(
      buffer->data() + 1, parameters_->data.c_str(), parameters_->data.size());
  resource->connection()->SendFeatureReport(
      buffer,
      buffer->size(),
      base::Bind(&HidSendFeatureReportFunction::OnFinished, this));
}

void HidSendFeatureReportFunction::OnFinished(bool success) {
  if (!success) {
    CompleteWithError(kErrorTransfer);
    return;
  }
  AsyncWorkCompleted();
}

}  // namespace extensions

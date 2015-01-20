// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/hid/hid_api.h"

#include <string>
#include <vector>

#include "base/stl_util.h"
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

const char kErrorServiceUnavailable[] = "HID services are unavailable.";
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

HidGetDevicesFunction::HidGetDevicesFunction() {}

HidGetDevicesFunction::~HidGetDevicesFunction() {}

ExtensionFunction::ResponseAction HidGetDevicesFunction::Run() {
  scoped_ptr<core_api::hid::GetDevices::Params> parameters =
      hid::GetDevices::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  HidDeviceManager* device_manager = HidDeviceManager::Get(browser_context());
  if (!device_manager) {
    return RespondNow(Error(kErrorServiceUnavailable));
  }

  std::vector<HidDeviceFilter> filters;
  if (parameters->options.filters) {
    filters.resize(parameters->options.filters->size());
    for (size_t i = 0; i < parameters->options.filters->size(); ++i) {
      ConvertHidDeviceFilter(parameters->options.filters->at(i), &filters[i]);
    }
  }
  if (parameters->options.vendor_id) {
    HidDeviceFilter legacy_filter;
    legacy_filter.SetVendorId(*parameters->options.vendor_id);
    if (parameters->options.product_id) {
      legacy_filter.SetProductId(*parameters->options.product_id);
    }
    filters.push_back(legacy_filter);
  }

  device_manager->GetApiDevices(
      extension(), filters,
      base::Bind(&HidGetDevicesFunction::OnEnumerationComplete, this));
  return RespondLater();
}

void HidGetDevicesFunction::OnEnumerationComplete(
    scoped_ptr<base::ListValue> devices) {
  Respond(OneArgument(devices.release()));
}

HidConnectFunction::HidConnectFunction() : connection_manager_(nullptr) {
}

HidConnectFunction::~HidConnectFunction() {}

ExtensionFunction::ResponseAction HidConnectFunction::Run() {
  scoped_ptr<core_api::hid::Connect::Params> parameters =
      hid::Connect::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  HidDeviceManager* device_manager = HidDeviceManager::Get(browser_context());
  if (!device_manager) {
    return RespondNow(Error(kErrorServiceUnavailable));
  }

  connection_manager_ =
      ApiResourceManager<HidConnectionResource>::Get(browser_context());
  if (!connection_manager_) {
    return RespondNow(Error(kErrorServiceUnavailable));
  }

  scoped_refptr<HidDeviceInfo> device_info =
      device_manager->GetDeviceInfo(parameters->device_id);
  if (!device_info) {
    return RespondNow(Error(kErrorInvalidDeviceId));
  }

  if (!HidDeviceManager::HasPermission(extension(), device_info)) {
    return RespondNow(Error(kErrorPermissionDenied));
  }

  HidService* hid_service = device::DeviceClient::Get()->GetHidService();
  if (!hid_service) {
    return RespondNow(Error(kErrorServiceUnavailable));
  }

  hid_service->Connect(
      device_info->device_id(),
      base::Bind(&HidConnectFunction::OnConnectComplete, this));
  return RespondLater();
}

void HidConnectFunction::OnConnectComplete(
    scoped_refptr<HidConnection> connection) {
  if (!connection.get()) {
    Respond(Error(kErrorFailedToOpenDevice));
    return;
  }

  DCHECK(connection_manager_);
  int connection_id = connection_manager_->Add(
      new HidConnectionResource(extension_id(), connection));
  Respond(OneArgument(PopulateHidConnection(connection_id, connection)));
}

HidDisconnectFunction::HidDisconnectFunction() {}

HidDisconnectFunction::~HidDisconnectFunction() {}

ExtensionFunction::ResponseAction HidDisconnectFunction::Run() {
  scoped_ptr<core_api::hid::Disconnect::Params> parameters =
      hid::Disconnect::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  ApiResourceManager<HidConnectionResource>* connection_manager =
      ApiResourceManager<HidConnectionResource>::Get(browser_context());
  if (!connection_manager) {
    return RespondNow(Error(kErrorServiceUnavailable));
  }

  int connection_id = parameters->connection_id;
  HidConnectionResource* resource =
      connection_manager->Get(extension_id(), connection_id);
  if (!resource) {
    return RespondNow(Error(kErrorConnectionNotFound));
  }

  connection_manager->Remove(extension_id(), connection_id);
  return RespondNow(NoArguments());
}

HidConnectionIoFunction::HidConnectionIoFunction() {
}

HidConnectionIoFunction::~HidConnectionIoFunction() {
}

ExtensionFunction::ResponseAction HidConnectionIoFunction::Run() {
  if (!ValidateParameters()) {
    return RespondNow(Error(error_));
  }

  ApiResourceManager<HidConnectionResource>* connection_manager =
      ApiResourceManager<HidConnectionResource>::Get(browser_context());
  if (!connection_manager) {
    return RespondNow(Error(kErrorServiceUnavailable));
  }

  HidConnectionResource* resource =
      connection_manager->Get(extension_id(), connection_id_);
  if (!resource) {
    return RespondNow(Error(kErrorConnectionNotFound));
  }

  StartWork(resource->connection().get());
  return RespondLater();
}

HidReceiveFunction::HidReceiveFunction() {}

HidReceiveFunction::~HidReceiveFunction() {}

bool HidReceiveFunction::ValidateParameters() {
  parameters_ = hid::Receive::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  set_connection_id(parameters_->connection_id);
  return true;
}

void HidReceiveFunction::StartWork(HidConnection* connection) {
  connection->Read(base::Bind(&HidReceiveFunction::OnFinished, this));
}

void HidReceiveFunction::OnFinished(bool success,
                                    scoped_refptr<net::IOBuffer> buffer,
                                    size_t size) {
  if (success) {
    DCHECK_GE(size, 1u);
    int report_id = reinterpret_cast<uint8_t*>(buffer->data())[0];

    Respond(TwoArguments(new base::FundamentalValue(report_id),
                         base::BinaryValue::CreateWithCopiedBuffer(
                             buffer->data() + 1, size - 1)));
  } else {
    Respond(Error(kErrorTransfer));
  }
}

HidSendFunction::HidSendFunction() {}

HidSendFunction::~HidSendFunction() {}

bool HidSendFunction::ValidateParameters() {
  parameters_ = hid::Send::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  set_connection_id(parameters_->connection_id);
  return true;
}

void HidSendFunction::StartWork(HidConnection* connection) {
  scoped_refptr<net::IOBufferWithSize> buffer(
      new net::IOBufferWithSize(parameters_->data.size() + 1));
  buffer->data()[0] = static_cast<uint8_t>(parameters_->report_id);
  memcpy(buffer->data() + 1, parameters_->data.data(),
         parameters_->data.size());
  connection->Write(buffer, buffer->size(),
                    base::Bind(&HidSendFunction::OnFinished, this));
}

void HidSendFunction::OnFinished(bool success) {
  if (success) {
    Respond(NoArguments());
  } else {
    Respond(Error(kErrorTransfer));
  }
}

HidReceiveFeatureReportFunction::HidReceiveFeatureReportFunction() {}

HidReceiveFeatureReportFunction::~HidReceiveFeatureReportFunction() {}

bool HidReceiveFeatureReportFunction::ValidateParameters() {
  parameters_ = hid::ReceiveFeatureReport::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  set_connection_id(parameters_->connection_id);
  return true;
}

void HidReceiveFeatureReportFunction::StartWork(HidConnection* connection) {
  connection->GetFeatureReport(
      static_cast<uint8_t>(parameters_->report_id),
      base::Bind(&HidReceiveFeatureReportFunction::OnFinished, this));
}

void HidReceiveFeatureReportFunction::OnFinished(
    bool success,
    scoped_refptr<net::IOBuffer> buffer,
    size_t size) {
  if (success) {
    Respond(OneArgument(
        base::BinaryValue::CreateWithCopiedBuffer(buffer->data(), size)));
  } else {
    Respond(Error(kErrorTransfer));
  }
}

HidSendFeatureReportFunction::HidSendFeatureReportFunction() {}

HidSendFeatureReportFunction::~HidSendFeatureReportFunction() {}

bool HidSendFeatureReportFunction::ValidateParameters() {
  parameters_ = hid::SendFeatureReport::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  set_connection_id(parameters_->connection_id);
  return true;
}

void HidSendFeatureReportFunction::StartWork(HidConnection* connection) {
  scoped_refptr<net::IOBufferWithSize> buffer(
      new net::IOBufferWithSize(parameters_->data.size() + 1));
  buffer->data()[0] = static_cast<uint8_t>(parameters_->report_id);
  memcpy(buffer->data() + 1, vector_as_array(&parameters_->data),
         parameters_->data.size());
  connection->SendFeatureReport(
      buffer, buffer->size(),
      base::Bind(&HidSendFeatureReportFunction::OnFinished, this));
}

void HidSendFeatureReportFunction::OnFinished(bool success) {
  if (success) {
    Respond(NoArguments());
  } else {
    Respond(Error(kErrorTransfer));
  }
}

}  // namespace extensions

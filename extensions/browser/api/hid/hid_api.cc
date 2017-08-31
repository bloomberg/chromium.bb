// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/hid/hid_api.h"

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "device/base/device_client.h"
#include "device/hid/hid_connection.h"
#include "device/hid/hid_device_filter.h"
#include "device/hid/hid_service.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/api/device_permissions_prompt.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/common/api/hid.h"
#include "net/base/io_buffer.h"

namespace hid = extensions::api::hid;

using device::HidConnection;
using device::HidDeviceFilter;
using device::HidService;

namespace {

const char kErrorPermissionDenied[] = "Permission to access device was denied.";
const char kErrorInvalidDeviceId[] = "Invalid HID device ID.";
const char kErrorFailedToOpenDevice[] = "Failed to open HID device.";
const char kErrorConnectionNotFound[] = "Connection not established.";
const char kErrorTransfer[] = "Transfer failed.";

std::unique_ptr<base::Value> PopulateHidConnection(
    int connection_id,
    scoped_refptr<HidConnection> connection) {
  hid::HidConnectInfo connection_value;
  connection_value.connection_id = connection_id;
  return connection_value.ToValue();
}

void ConvertHidDeviceFilter(const hid::DeviceFilter& input,
                            HidDeviceFilter* output) {
  if (input.vendor_id) {
    output->SetVendorId(*input.vendor_id);
  }
  if (input.product_id) {
    output->SetProductId(*input.product_id);
  }
  if (input.usage_page) {
    output->SetUsagePage(*input.usage_page);
  }
  if (input.usage) {
    output->SetUsage(*input.usage);
  }
}

}  // namespace

namespace extensions {

HidGetDevicesFunction::HidGetDevicesFunction() {}

HidGetDevicesFunction::~HidGetDevicesFunction() {}

ExtensionFunction::ResponseAction HidGetDevicesFunction::Run() {
  std::unique_ptr<api::hid::GetDevices::Params> parameters =
      hid::GetDevices::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters);

  HidDeviceManager* device_manager = HidDeviceManager::Get(browser_context());
  CHECK(device_manager);

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
    std::unique_ptr<base::ListValue> devices) {
  Respond(OneArgument(std::move(devices)));
}

HidGetUserSelectedDevicesFunction::HidGetUserSelectedDevicesFunction() {
}

HidGetUserSelectedDevicesFunction::~HidGetUserSelectedDevicesFunction() {
}

ExtensionFunction::ResponseAction HidGetUserSelectedDevicesFunction::Run() {
  std::unique_ptr<api::hid::GetUserSelectedDevices::Params> parameters =
      hid::GetUserSelectedDevices::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters);

  content::WebContents* web_contents = GetSenderWebContents();
  if (!web_contents || !user_gesture()) {
    return RespondNow(OneArgument(std::make_unique<base::ListValue>()));
  }

  bool multiple = false;
  std::vector<HidDeviceFilter> filters;
  if (parameters->options) {
    multiple = parameters->options->multiple && *parameters->options->multiple;
    if (parameters->options->filters) {
      const auto& api_filters = *parameters->options->filters;
      filters.resize(api_filters.size());
      for (size_t i = 0; i < api_filters.size(); ++i) {
        ConvertHidDeviceFilter(api_filters[i], &filters[i]);
      }
    }
  }

  prompt_ =
      ExtensionsAPIClient::Get()->CreateDevicePermissionsPrompt(web_contents);
  CHECK(prompt_);
  prompt_->AskForHidDevices(
      extension(), browser_context(), multiple, filters,
      base::Bind(&HidGetUserSelectedDevicesFunction::OnDevicesChosen, this));
  return RespondLater();
}

void HidGetUserSelectedDevicesFunction::OnDevicesChosen(
    std::vector<device::mojom::HidDeviceInfoPtr> devices) {
  HidDeviceManager* device_manager = HidDeviceManager::Get(browser_context());
  CHECK(device_manager);
  Respond(
      OneArgument(device_manager->GetApiDevicesFromList(std::move(devices))));
}

HidConnectFunction::HidConnectFunction() : connection_manager_(nullptr) {
}

HidConnectFunction::~HidConnectFunction() {}

ExtensionFunction::ResponseAction HidConnectFunction::Run() {
  std::unique_ptr<api::hid::Connect::Params> parameters =
      hid::Connect::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters);

  HidDeviceManager* device_manager = HidDeviceManager::Get(browser_context());
  CHECK(device_manager);

  connection_manager_ =
      ApiResourceManager<HidConnectionResource>::Get(browser_context());
  CHECK(connection_manager_);

  const device::mojom::HidDeviceInfo* device_info =
      device_manager->GetDeviceInfo(parameters->device_id);
  if (!device_info) {
    return RespondNow(Error(kErrorInvalidDeviceId));
  }

  if (!device_manager->HasPermission(extension(), *device_info, true)) {
    return RespondNow(Error(kErrorPermissionDenied));
  }

  HidService* hid_service = device::DeviceClient::Get()->GetHidService();
  CHECK(hid_service);

  hid_service->Connect(
      device_info->guid,
      base::Bind(&HidConnectFunction::OnConnectComplete, this));
  return RespondLater();
}

void HidConnectFunction::OnConnectComplete(
    scoped_refptr<HidConnection> connection) {
  if (!connection) {
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
  std::unique_ptr<api::hid::Disconnect::Params> parameters =
      hid::Disconnect::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters);

  ApiResourceManager<HidConnectionResource>* connection_manager =
      ApiResourceManager<HidConnectionResource>::Get(browser_context());
  CHECK(connection_manager);

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
  EXTENSION_FUNCTION_VALIDATE(ReadParameters());

  ApiResourceManager<HidConnectionResource>* connection_manager =
      ApiResourceManager<HidConnectionResource>::Get(browser_context());
  CHECK(connection_manager);

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

bool HidReceiveFunction::ReadParameters() {
  parameters_ = hid::Receive::Params::Create(*args_);
  if (!parameters_)
    return false;
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

    Respond(TwoArguments(
        std::make_unique<base::Value>(report_id),
        base::Value::CreateWithCopiedBuffer(buffer->data() + 1, size - 1)));
  } else {
    Respond(Error(kErrorTransfer));
  }
}

HidSendFunction::HidSendFunction() {}

HidSendFunction::~HidSendFunction() {}

bool HidSendFunction::ReadParameters() {
  parameters_ = hid::Send::Params::Create(*args_);
  if (!parameters_)
    return false;
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

bool HidReceiveFeatureReportFunction::ReadParameters() {
  parameters_ = hid::ReceiveFeatureReport::Params::Create(*args_);
  if (!parameters_)
    return false;
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
    Respond(
        OneArgument(base::Value::CreateWithCopiedBuffer(buffer->data(), size)));
  } else {
    Respond(Error(kErrorTransfer));
  }
}

HidSendFeatureReportFunction::HidSendFeatureReportFunction() {}

HidSendFeatureReportFunction::~HidSendFeatureReportFunction() {}

bool HidSendFeatureReportFunction::ReadParameters() {
  parameters_ = hid::SendFeatureReport::Params::Create(*args_);
  if (!parameters_)
    return false;
  set_connection_id(parameters_->connection_id);
  return true;
}

void HidSendFeatureReportFunction::StartWork(HidConnection* connection) {
  scoped_refptr<net::IOBufferWithSize> buffer(
      new net::IOBufferWithSize(parameters_->data.size() + 1));
  buffer->data()[0] = static_cast<uint8_t>(parameters_->report_id);
  memcpy(buffer->data() + 1, parameters_->data.data(),
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

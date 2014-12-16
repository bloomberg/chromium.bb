// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/webrtc/devtools_bridge_instances_request.h"

#include "base/values.h"
#include "components/cloud_devices/common/cloud_devices_urls.h"
#include "net/base/url_util.h"

namespace {

std::string GetKind(const base::DictionaryValue& value) {
  std::string result;
  value.GetString("kind", &result);
  return result;
}

bool HasCommand(const base::DictionaryValue& commands_defs_value,
                const std::string& command_name) {
  const base::DictionaryValue* command_value;
  return commands_defs_value.GetDictionary(command_name, &command_value) &&
         GetKind(*command_value) == "clouddevices#commandDef";
}

}  // namespace

DevToolsBridgeInstancesRequest::Instance::~Instance() {
}

DevToolsBridgeInstancesRequest::DevToolsBridgeInstancesRequest(
    Delegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

DevToolsBridgeInstancesRequest::~DevToolsBridgeInstancesRequest() {
}

void DevToolsBridgeInstancesRequest::OnGCDAPIFlowError(
    local_discovery::GCDApiFlow::Status status) {
  delegate_->OnDevToolsBridgeInstancesRequestFailed();
}

void DevToolsBridgeInstancesRequest::OnGCDAPIFlowComplete(
    const base::DictionaryValue& value) {
  const base::ListValue* device_list_value = NULL;
  if (GetKind(value) == "clouddevices#devicesListResponse" &&
      value.GetList("devices", &device_list_value)) {
    for (const auto& device_value : *device_list_value) {
      const base::DictionaryValue* dictionary;
      if (device_value->GetAsDictionary(&dictionary))
        TryAddInstance(*dictionary);
    }
  }

  delegate_->OnDevToolsBridgeInstancesRequestSucceeded(result_);
}

GURL DevToolsBridgeInstancesRequest::GetURL() {
  return cloud_devices::GetCloudDevicesRelativeURL("devices");
}

void DevToolsBridgeInstancesRequest::TryAddInstance(
    const base::DictionaryValue& device_value) {
  if (GetKind(device_value) != "clouddevices#device")
    return;

  const base::DictionaryValue* commands_defs_value;
  if (!device_value.GetDictionary("commandDefs", &commands_defs_value))
    return;

  if (!HasCommand(*commands_defs_value, "base._startSession") ||
      !HasCommand(*commands_defs_value, "base._iceExchange") ||
      !HasCommand(*commands_defs_value, "base._renegotiate")) {
    return;
  }

  Instance instance;
  if (!device_value.GetString("id", &instance.id))
    return;
  if (!device_value.GetString("displayName", &instance.display_name))
    return;

  result_.push_back(instance);
}

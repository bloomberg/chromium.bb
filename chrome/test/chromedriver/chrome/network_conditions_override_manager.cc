// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/network_conditions_override_manager.h"

#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/network_conditions.h"
#include "chrome/test/chromedriver/chrome/status.h"

NetworkConditionsOverrideManager::NetworkConditionsOverrideManager(
    DevToolsClient* client)
    : client_(client),
      overridden_network_conditions_(NULL) {
  client_->AddListener(this);
}

NetworkConditionsOverrideManager::~NetworkConditionsOverrideManager() {
}

Status NetworkConditionsOverrideManager::OverrideNetworkConditions(
    const NetworkConditions& network_conditions) {
  Status status = ApplyOverride(&network_conditions);
  if (status.IsOk())
    overridden_network_conditions_ = &network_conditions;
  return status;
}

Status NetworkConditionsOverrideManager::OnConnected(DevToolsClient* client) {
  return ApplyOverrideIfNeeded();
}

Status NetworkConditionsOverrideManager::OnEvent(
    DevToolsClient* client,
    const std::string& method,
    const base::DictionaryValue& params) {
  if (method == "Page.frameNavigated") {
    const base::Value* unused_value;
    if (!params.Get("frame.parentId", &unused_value))
      return ApplyOverrideIfNeeded();
  }
  return Status(kOk);
}

Status NetworkConditionsOverrideManager::ApplyOverrideIfNeeded() {
  if (overridden_network_conditions_)
    return ApplyOverride(overridden_network_conditions_);
  return Status(kOk);
}

Status NetworkConditionsOverrideManager::ApplyOverride(
    const NetworkConditions* network_conditions) {
  base::DictionaryValue params, empty_params;
  params.SetBoolean("offline", network_conditions->offline);
  params.SetDouble("latency", network_conditions->latency);
  params.SetDouble("downloadThroughput",
                    network_conditions->download_throughput);
  params.SetDouble("uploadThroughput", network_conditions->upload_throughput);

  Status status = client_->SendCommand("Network.enable", empty_params);
  if (status.IsError())
    return status;

  std::unique_ptr<base::DictionaryValue> result;
  bool can = false;
  status = client_->SendCommandAndGetResult(
      "Network.canEmulateNetworkConditions", empty_params, &result);
  if (status.IsError() || !result->GetBoolean("result", &can))
    return Status(kUnknownError,
        "unable to detect if chrome can emulate network conditions", status);
  if (!can)
    return Status(kUnknownError, "Cannot emulate network conditions");

  return client_->SendCommand("Network.emulateNetworkConditions", params);
}

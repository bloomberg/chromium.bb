// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/devtools/devtools_network_controller.h"

#include <utility>

#include "content/common/devtools/devtools_network_conditions.h"
#include "content/common/devtools/devtools_network_interceptor.h"
#include "net/http/http_request_info.h"

namespace content {

DevToolsNetworkController* DevToolsNetworkController::instance_ = nullptr;

DevToolsNetworkController::DevToolsNetworkController() = default;
DevToolsNetworkController::~DevToolsNetworkController() = default;

// static
DevToolsNetworkInterceptor* DevToolsNetworkController::GetInterceptor(
    const std::string& client_id) {
  if (!instance_ || client_id.empty())
    return nullptr;
  return instance_->FindInterceptor(client_id);
}

// static
void DevToolsNetworkController::SetNetworkState(
    const std::string& client_id,
    std::unique_ptr<DevToolsNetworkConditions> conditions) {
  if (!instance_) {
    if (!conditions)
      return;
    instance_ = new DevToolsNetworkController();
  }
  instance_->SetConditions(client_id, std::move(conditions));
}

DevToolsNetworkInterceptor* DevToolsNetworkController::FindInterceptor(
    const std::string& client_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto it = interceptors_.find(client_id);
  return it != interceptors_.end() ? it->second.get() : nullptr;
}

void DevToolsNetworkController::SetConditions(
    const std::string& client_id,
    std::unique_ptr<DevToolsNetworkConditions> conditions) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto it = interceptors_.find(client_id);
  if (it == interceptors_.end()) {
    if (!conditions)
      return;
    std::unique_ptr<DevToolsNetworkInterceptor> new_interceptor(
        new DevToolsNetworkInterceptor());
    new_interceptor->UpdateConditions(std::move(conditions));
    interceptors_[client_id] = std::move(new_interceptor);
  } else {
    if (!conditions) {
      std::unique_ptr<DevToolsNetworkConditions> online_conditions(
          new DevToolsNetworkConditions());
      it->second->UpdateConditions(std::move(online_conditions));
      interceptors_.erase(client_id);
      if (interceptors_.empty()) {
        delete this;
        instance_ = nullptr;
      }
    } else {
      it->second->UpdateConditions(std::move(conditions));
    }
  }
}

}  // namespace content

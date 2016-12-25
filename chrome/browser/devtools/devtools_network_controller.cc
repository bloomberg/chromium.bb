// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_network_controller.h"

#include <utility>

#include "chrome/browser/devtools/devtools_network_conditions.h"
#include "chrome/browser/devtools/devtools_network_interceptor.h"
#include "net/http/http_request_info.h"

DevToolsNetworkController::DevToolsNetworkController()
    : appcache_interceptor_(new DevToolsNetworkInterceptor()) {}

DevToolsNetworkController::~DevToolsNetworkController() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

DevToolsNetworkInterceptor* DevToolsNetworkController::GetInterceptor(
    const std::string& client_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (interceptors_.empty() || client_id.empty())
    return nullptr;

  auto it = interceptors_.find(client_id);
  if (it == interceptors_.end())
    return nullptr;

  return it->second.get();
}

void DevToolsNetworkController::SetNetworkState(
    const std::string& client_id,
    std::unique_ptr<DevToolsNetworkConditions> conditions) {
  DCHECK(thread_checker_.CalledOnValidThread());

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
    } else {
      it->second->UpdateConditions(std::move(conditions));
    }
  }

  bool has_offline_interceptors = false;
  for (const auto& interceptor : interceptors_) {
    if (interceptor.second->IsOffline()) {
      has_offline_interceptors = true;
      break;
    }
  }

  bool is_appcache_offline = appcache_interceptor_->IsOffline();
  if (is_appcache_offline != has_offline_interceptors) {
    std::unique_ptr<DevToolsNetworkConditions> appcache_conditions(
        new DevToolsNetworkConditions(has_offline_interceptors));
    appcache_interceptor_->UpdateConditions(std::move(appcache_conditions));
  }
}

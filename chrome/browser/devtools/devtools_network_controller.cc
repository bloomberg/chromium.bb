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
  if (!interceptors_.size() || client_id.empty())
    return nullptr;

  DevToolsNetworkInterceptor* interceptor = interceptors_.get(client_id);
  if (!interceptor)
    return nullptr;

  return interceptor;
}

void DevToolsNetworkController::SetNetworkState(
    const std::string& client_id,
    scoped_ptr<DevToolsNetworkConditions> conditions) {
  DCHECK(thread_checker_.CalledOnValidThread());

  DevToolsNetworkInterceptor* interceptor = interceptors_.get(client_id);
  if (!interceptor) {
    DCHECK(conditions);
    if (!conditions)
      return;
    scoped_ptr<DevToolsNetworkInterceptor> new_interceptor(
        new DevToolsNetworkInterceptor());
    new_interceptor->UpdateConditions(std::move(conditions));
    interceptors_.set(client_id, std::move(new_interceptor));
  } else {
    if (!conditions) {
      scoped_ptr<DevToolsNetworkConditions> online_conditions(
          new DevToolsNetworkConditions());
      interceptor->UpdateConditions(std::move(online_conditions));
      interceptors_.erase(client_id);
    } else {
      interceptor->UpdateConditions(std::move(conditions));
    }
  }

  bool has_offline_interceptors = false;
  InterceptorMap::iterator it = interceptors_.begin();
  for (; it != interceptors_.end(); ++it) {
    if (it->second->IsOffline()) {
      has_offline_interceptors = true;
      break;
    }
  }

  bool is_appcache_offline = appcache_interceptor_->IsOffline();
  if (is_appcache_offline != has_offline_interceptors) {
    scoped_ptr<DevToolsNetworkConditions> appcache_conditions(
        new DevToolsNetworkConditions(has_offline_interceptors));
    appcache_interceptor_->UpdateConditions(std::move(appcache_conditions));
  }
}

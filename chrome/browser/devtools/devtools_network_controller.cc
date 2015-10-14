// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_network_controller.h"

#include "chrome/browser/devtools/devtools_network_conditions.h"
#include "chrome/browser/devtools/devtools_network_interceptor.h"
#include "chrome/browser/devtools/devtools_network_transaction.h"
#include "net/http/http_request_info.h"

DevToolsNetworkController::DevToolsNetworkController()
    : default_interceptor_(new DevToolsNetworkInterceptor()),
      appcache_interceptor_(new DevToolsNetworkInterceptor()) {}

DevToolsNetworkController::~DevToolsNetworkController() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

base::WeakPtr<DevToolsNetworkInterceptor>
DevToolsNetworkController::GetInterceptor(
    DevToolsNetworkTransaction* transaction) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(transaction->request());

  if (!interceptors_.size())
    return default_interceptor_->GetWeakPtr();

  transaction->ProcessRequest();

  const std::string& client_id = transaction->client_id();
  if (client_id.empty())
    return default_interceptor_->GetWeakPtr();

  DevToolsNetworkInterceptor* interceptor = interceptors_.get(client_id);
  if (!interceptor)
    return default_interceptor_->GetWeakPtr();

  return interceptor->GetWeakPtr();
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
    new_interceptor->UpdateConditions(conditions.Pass());
    interceptors_.set(client_id, new_interceptor.Pass());
  } else {
    if (!conditions) {
      scoped_ptr<DevToolsNetworkConditions> online_conditions(
          new DevToolsNetworkConditions());
      interceptor->UpdateConditions(online_conditions.Pass());
      interceptors_.erase(client_id);
    } else {
      interceptor->UpdateConditions(conditions.Pass());
    }
  }

  bool has_offline_interceptors = false;
  InterceptorMap::iterator it = interceptors_.begin();
  for (; it != interceptors_.end(); ++it) {
    if (it->second->conditions()->offline()) {
      has_offline_interceptors = true;
      break;
    }
  }

  bool is_appcache_offline = appcache_interceptor_->conditions()->offline();
  if (is_appcache_offline != has_offline_interceptors) {
    scoped_ptr<DevToolsNetworkConditions> appcache_conditions(
        new DevToolsNetworkConditions(has_offline_interceptors));
    appcache_interceptor_->UpdateConditions(appcache_conditions.Pass());
  }
}

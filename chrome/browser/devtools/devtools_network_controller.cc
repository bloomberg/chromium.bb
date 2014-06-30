// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_network_controller.h"

#include "chrome/browser/devtools/devtools_network_conditions.h"
#include "chrome/browser/devtools/devtools_network_interceptor.h"
#include "chrome/browser/devtools/devtools_network_transaction.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_info.h"

using content::BrowserThread;

DevToolsNetworkController::DevToolsNetworkController()
    : default_interceptor_(new DevToolsNetworkInterceptor()),
      appcache_interceptor_(new DevToolsNetworkInterceptor()),
      weak_ptr_factory_(this) {
}

DevToolsNetworkController::~DevToolsNetworkController() {
}

base::WeakPtr<DevToolsNetworkInterceptor>
DevToolsNetworkController::GetInterceptor(
    DevToolsNetworkTransaction* transaction) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(transaction->request());

  if (!interceptors_.size())
    return default_interceptor_->GetWeakPtr();

  if (transaction->request()->load_flags & net::LOAD_DISABLE_INTERCEPT)
    return appcache_interceptor_->GetWeakPtr();

  transaction->ProcessRequest();

  const std::string& client_id = transaction->client_id();
  if (client_id.empty())
    return default_interceptor_->GetWeakPtr();

  DevToolsNetworkInterceptor* interceptor = interceptors_.get(client_id);
  DCHECK(interceptor);
  if (!interceptor)
    return default_interceptor_->GetWeakPtr();

  return interceptor->GetWeakPtr();
}

void DevToolsNetworkController::SetNetworkState(
    const std::string& client_id,
    scoped_ptr<DevToolsNetworkConditions> conditions) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &DevToolsNetworkController::SetNetworkStateOnIO,
          weak_ptr_factory_.GetWeakPtr(),
          client_id,
          base::Passed(&conditions)));
}

void DevToolsNetworkController::SetNetworkStateOnIO(
    const std::string& client_id,
    scoped_ptr<DevToolsNetworkConditions> conditions) {
  DCHECK(thread_checker_.CalledOnValidThread());

  DevToolsNetworkInterceptor* interceptor = interceptors_.get(client_id);
  if (!interceptor) {
    DCHECK(conditions);
    if (!conditions)
      return;
    Interceptor new_interceptor = Interceptor(new DevToolsNetworkInterceptor());
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
  Interceptors::iterator it = interceptors_.begin();
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

// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_job.h"

#include "base/command_line.h"
#include "content/browser/appcache/appcache_request.h"
#include "content/browser/appcache/appcache_url_loader_job.h"
#include "content/browser/appcache/appcache_url_request_job.h"

#include "content/public/common/content_switches.h"

namespace content {

std::unique_ptr<AppCacheJob> AppCacheJob::Create(
    bool is_main_resource,
    AppCacheHost* host,
    AppCacheStorage* storage,
    AppCacheRequest* request,
    net::NetworkDelegate* network_delegate,
    const OnPrepareToRestartCallback& restart_callback) {
  std::unique_ptr<AppCacheJob> job;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableNetworkService)) {
    job.reset(
        new AppCacheURLLoaderJob(*(request->GetResourceRequest()), storage));
  } else {
    job.reset(new AppCacheURLRequestJob(request->GetURLRequest(),
                                        network_delegate, storage, host,
                                        is_main_resource, restart_callback));
  }
  return job;
}

AppCacheJob::~AppCacheJob() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool AppCacheJob::IsWaiting() const {
  return delivery_type_ == AWAITING_DELIVERY_ORDERS;
}

bool AppCacheJob::IsDeliveringAppCacheResponse() const {
  return delivery_type_ == APPCACHED_DELIVERY;
}

bool AppCacheJob::IsDeliveringNetworkResponse() const {
  return delivery_type_ == NETWORK_DELIVERY;
}

bool AppCacheJob::IsDeliveringErrorResponse() const {
  return delivery_type_ == ERROR_DELIVERY;
}

bool AppCacheJob::IsCacheEntryNotFound() const {
  return cache_entry_not_found_;
}

base::WeakPtr<AppCacheJob> AppCacheJob::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

net::URLRequestJob* AppCacheJob::AsURLRequestJob() {
  return nullptr;
}

AppCacheURLLoaderJob* AppCacheJob::AsURLLoaderJob() {
  return nullptr;
}

AppCacheJob::AppCacheJob()
    : cache_entry_not_found_(false),
      delivery_type_(AWAITING_DELIVERY_ORDERS),
      weak_factory_(this) {}

}  // namespace content

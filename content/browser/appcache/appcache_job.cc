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
    job.reset(new AppCacheURLLoaderJob);
  } else {
    job.reset(new AppCacheURLRequestJob(request->GetURLRequest(),
                                        network_delegate, storage, host,
                                        is_main_resource, restart_callback));
  }
  return job;
}

AppCacheJob::~AppCacheJob() {}

base::WeakPtr<AppCacheJob> AppCacheJob::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

net::URLRequestJob* AppCacheJob::AsURLRequestJob() {
  return nullptr;
}

AppCacheURLLoaderJob* AppCacheJob::AsURLLoaderJob() {
  return nullptr;
}

AppCacheJob::AppCacheJob() : weak_factory_(this) {}

}  // namespace content

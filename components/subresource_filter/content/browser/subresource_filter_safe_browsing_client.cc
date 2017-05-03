// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_client.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_activation_throttle.h"
#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_client_request.h"
#include "content/public/browser/browser_thread.h"

namespace subresource_filter {

SubresourceFilterSafeBrowsingClient::SubresourceFilterSafeBrowsingClient(
    scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager,
    base::WeakPtr<SubresourceFilterSafeBrowsingActivationThrottle> throttle,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> throttle_task_runner)
    : database_manager_(std::move(database_manager)),
      throttle_(std::move(throttle)),
      io_task_runner_(std::move(io_task_runner)),
      throttle_task_runner_(std::move(throttle_task_runner)) {}

SubresourceFilterSafeBrowsingClient::~SubresourceFilterSafeBrowsingClient() {}

void SubresourceFilterSafeBrowsingClient::CheckUrlOnIO(const GURL& url,
                                                       size_t request_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!url.is_empty());

  auto request = base::MakeUnique<SubresourceFilterSafeBrowsingClientRequest>(
      url, request_id, database_manager_, io_task_runner_, this);
  auto* raw_request = request.get();
  DCHECK(requests_.find(raw_request) == requests_.end());
  requests_[raw_request] = std::move(request);
  raw_request->Start();
  // Careful, |raw_request| can be destroyed after this line.
}

void SubresourceFilterSafeBrowsingClient::OnCheckBrowseUrlResult(
    SubresourceFilterSafeBrowsingClientRequest* request,
    const CheckResult& check_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  throttle_task_runner_->PostTask(
      FROM_HERE, base::Bind(&SubresourceFilterSafeBrowsingActivationThrottle::
                                OnCheckUrlResultOnUI,
                            throttle_, check_result));

  DCHECK(requests_.find(request) != requests_.end());
  requests_.erase(request);
}

}  // namespace subresource_filter

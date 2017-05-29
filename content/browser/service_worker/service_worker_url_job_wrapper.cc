// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_url_job_wrapper.h"

#include "base/command_line.h"
#include "content/browser/service_worker/service_worker_response_type.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_switches.h"

namespace content {

ServiceWorkerURLJobWrapper::ServiceWorkerURLJobWrapper(
    base::WeakPtr<ServiceWorkerURLRequestJob> url_request_job)
    : job_type_(JobType::kURLRequest),
      url_request_job_(std::move(url_request_job)),
      weak_factory_(this) {}

ServiceWorkerURLJobWrapper::ServiceWorkerURLJobWrapper(
    LoaderFactoryCallback callback)
    : job_type_(JobType::kURLLoader),
      loader_factory_callback_(std::move(callback)),
      weak_factory_(this) {
  DCHECK(IsBrowserSideNavigationEnabled() &&
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kEnableNetworkService));
}

ServiceWorkerURLJobWrapper::~ServiceWorkerURLJobWrapper() {}

void ServiceWorkerURLJobWrapper::FallbackToNetwork() {
  if (job_type_ == JobType::kURLLoader) {
    response_type_ = FALLBACK_TO_NETWORK;
    // This could be called multiple times in some cases because we simply
    // call this synchronously here and don't wait for a separate async
    // StartRequest cue like what URLRequestJob case does.
    // TODO(kinuko): Make sure this is ok or we need to make this async.
    if (!loader_factory_callback_.is_null()) {
      std::move(loader_factory_callback_).Run(nullptr);
    }
  } else {
    url_request_job_->FallbackToNetwork();
  }
}

void ServiceWorkerURLJobWrapper::FallbackToNetworkOrRenderer() {
  if (job_type_ == JobType::kURLLoader) {
    // TODO(kinuko): Implement this. Now we always fallback to network.
    FallbackToNetwork();
  } else {
    url_request_job_->FallbackToNetworkOrRenderer();
  }
}

void ServiceWorkerURLJobWrapper::ForwardToServiceWorker() {
  if (job_type_ == JobType::kURLLoader) {
    response_type_ = FORWARD_TO_SERVICE_WORKER;
    StartRequest();
  } else {
    url_request_job_->ForwardToServiceWorker();
  }
}

bool ServiceWorkerURLJobWrapper::ShouldFallbackToNetwork() {
  if (job_type_ == JobType::kURLLoader) {
    return response_type_ == FALLBACK_TO_NETWORK;
  } else {
    return url_request_job_->ShouldFallbackToNetwork();
  }
}

ui::PageTransition ServiceWorkerURLJobWrapper::GetPageTransition() {
  if (job_type_ == JobType::kURLLoader) {
    NOTIMPLEMENTED();
    return ui::PAGE_TRANSITION_LINK;
  } else {
    const ResourceRequestInfo* info =
        ResourceRequestInfo::ForRequest(url_request_job_->request());
    // ResourceRequestInfo may not be set in some tests.
    if (!info)
      return ui::PAGE_TRANSITION_LINK;
    return info->GetPageTransition();
  }
}

size_t ServiceWorkerURLJobWrapper::GetURLChainSize() const {
  if (job_type_ == JobType::kURLLoader) {
    NOTIMPLEMENTED();
    return 0;
  } else {
    return url_request_job_->request()->url_chain().size();
  }
}

void ServiceWorkerURLJobWrapper::FailDueToLostController() {
  if (job_type_ == JobType::kURLLoader) {
    NOTIMPLEMENTED();
  } else {
    url_request_job_->FailDueToLostController();
  }
}

bool ServiceWorkerURLJobWrapper::WasCanceled() const {
  if (job_type_ == JobType::kURLLoader) {
    return loader_factory_callback_.is_null();
  } else {
    return !url_request_job_;
  }
}

void ServiceWorkerURLJobWrapper::StartRequest() {
  DCHECK_EQ(FORWARD_TO_SERVICE_WORKER, response_type_);
  // TODO(kinuko): Implement. For now we just exercise async fall back path
  // to the network.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&ServiceWorkerURLJobWrapper::FallbackToNetwork,
                            weak_factory_.GetWeakPtr()));
}

}  // namespace content

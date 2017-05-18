// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_url_job_wrapper.h"

#include "base/command_line.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_switches.h"

namespace content {

ServiceWorkerURLJobWrapper::ServiceWorkerURLJobWrapper(
    base::WeakPtr<ServiceWorkerURLRequestJob> url_request_job)
    : url_request_job_(std::move(url_request_job)), url_loader_(nullptr) {}

ServiceWorkerURLJobWrapper::ServiceWorkerURLJobWrapper(
    ServiceWorkerControlleeURLLoader* url_loader)
    : url_loader_(url_loader) {
  DCHECK(IsBrowserSideNavigationEnabled() &&
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kEnableNetworkService));
}

ServiceWorkerURLJobWrapper::~ServiceWorkerURLJobWrapper() {}

void ServiceWorkerURLJobWrapper::FallbackToNetwork() {
  if (url_loader_) {
    NOTIMPLEMENTED();
  } else {
    url_request_job_->FallbackToNetwork();
  }
}

void ServiceWorkerURLJobWrapper::FallbackToNetworkOrRenderer() {
  if (url_loader_) {
    NOTIMPLEMENTED();
  } else {
    url_request_job_->FallbackToNetworkOrRenderer();
  }
}

void ServiceWorkerURLJobWrapper::ForwardToServiceWorker() {
  if (url_loader_) {
    NOTIMPLEMENTED();
  } else {
    url_request_job_->ForwardToServiceWorker();
  }
}

bool ServiceWorkerURLJobWrapper::ShouldFallbackToNetwork() {
  if (url_loader_) {
    NOTIMPLEMENTED();
    return false;
  } else {
    return url_request_job_->ShouldFallbackToNetwork();
  }
}

ui::PageTransition ServiceWorkerURLJobWrapper::GetPageTransition() {
  if (url_loader_) {
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
  if (url_loader_) {
    NOTIMPLEMENTED();
    return 0;
  } else {
    return url_request_job_->request()->url_chain().size();
  }
}

void ServiceWorkerURLJobWrapper::FailDueToLostController() {
  if (url_loader_) {
    NOTIMPLEMENTED();
  } else {
    url_request_job_->FailDueToLostController();
  }
}

bool ServiceWorkerURLJobWrapper::WasCanceled() const {
  if (url_loader_) {
    NOTIMPLEMENTED();
    return true;
  } else {
    return !url_request_job_;
  }
}

}  // namespace content

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chromeos_merge_session_loader_throttle.h"

#include <utility>

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/renderer/chrome_render_thread_observer.h"

// static
base::TimeDelta MergeSessionLoaderThrottle::GetMergeSessionTimeout() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kShortMergeSessionTimeoutForTest)) {
    return base::TimeDelta::FromSeconds(1);
  } else {
    return base::TimeDelta::FromSeconds(10);
  }
}

MergeSessionLoaderThrottle::MergeSessionLoaderThrottle(
    base::WeakPtr<ChromeRenderThreadObserver> render_thread_observer)
    : render_thread_observer_(std::move(render_thread_observer)),
      weak_ptr_factory_(this) {}

MergeSessionLoaderThrottle::~MergeSessionLoaderThrottle() = default;

bool MergeSessionLoaderThrottle::MaybeDeferForMergeSession(
    const GURL& url,
    DelayedCallbackGroup::Callback resume_callback) {
  if (!render_thread_observer_->IsMergeSessionRunning())
    return false;

  render_thread_observer_->RunWhenMergeSessionFinished(
      std::move(resume_callback));
  return true;
}

void MergeSessionLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  is_xhr_ = request->resource_type == content::RESOURCE_TYPE_XHR;
  if (is_xhr_ && request->url.SchemeIsHTTPOrHTTPS() &&
      MaybeDeferForMergeSession(
          request->url,
          base::BindOnce(&MergeSessionLoaderThrottle::ResumeLoader,
                         weak_ptr_factory_.GetWeakPtr()))) {
    *defer = true;
  }
}

void MergeSessionLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::ResourceResponseHead& /* response_head */,
    bool* defer,
    std::vector<std::string>* to_be_removed_headers,
    net::HttpRequestHeaders* modified_headers) {
  if (is_xhr_ && redirect_info->new_url.SchemeIsHTTPOrHTTPS() &&
      MaybeDeferForMergeSession(
          redirect_info->new_url,
          base::BindOnce(&MergeSessionLoaderThrottle::ResumeLoader,
                         weak_ptr_factory_.GetWeakPtr()))) {
    *defer = true;
  }
}

void MergeSessionLoaderThrottle::DetachFromCurrentSequence() {}

void MergeSessionLoaderThrottle::ResumeLoader(
    DelayedCallbackGroup::RunReason run_reason) {
  LOG_IF(ERROR, run_reason == DelayedCallbackGroup::RunReason::TIMEOUT)
      << "Merge session loader throttle timeout.";
  DVLOG(1) << "Resuming deferred XHR request.";
  delegate_->Resume();
}

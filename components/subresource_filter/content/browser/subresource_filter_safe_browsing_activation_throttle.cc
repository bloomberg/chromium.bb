// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_activation_throttle.h"

#include <utility>
#include <vector>

#include "base/metrics/histogram_macros.h"
#include "base/timer/timer.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_driver_factory.h"
#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace subresource_filter {

SubresourceFilterSafeBrowsingActivationThrottle::
    SubresourceFilterSafeBrowsingActivationThrottle(
        content::NavigationHandle* handle,
        scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
        scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
            database_manager)
    : NavigationThrottle(handle),
      database_manager_(std::move(database_manager)),
      io_task_runner_(io_task_runner),
      database_client_(new SubresourceFilterSafeBrowsingClient(
                           database_manager_,
                           AsWeakPtr(),
                           io_task_runner,
                           base::ThreadTaskRunnerHandle::Get()),
                       base::OnTaskRunnerDeleter(io_task_runner_)) {}

SubresourceFilterSafeBrowsingActivationThrottle::
    ~SubresourceFilterSafeBrowsingActivationThrottle() {
  // TODO(csharrison): Log metrics based on check_results_.
}

content::NavigationThrottle::ThrottleCheckResult
SubresourceFilterSafeBrowsingActivationThrottle::WillStartRequest() {
  CheckCurrentUrl();
  return content::NavigationThrottle::ThrottleCheckResult::PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
SubresourceFilterSafeBrowsingActivationThrottle::WillRedirectRequest() {
  CheckCurrentUrl();
  return content::NavigationThrottle::ThrottleCheckResult::PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
SubresourceFilterSafeBrowsingActivationThrottle::WillProcessResponse() {
  // No need to defer the navigation if the check already happened.
  if (check_results_.back().finished) {
    NotifyResult();
    return content::NavigationThrottle::ThrottleCheckResult::PROCEED;
  }
  defer_time_ = base::TimeTicks::Now();
  return content::NavigationThrottle::ThrottleCheckResult::DEFER;
}

const char*
SubresourceFilterSafeBrowsingActivationThrottle::GetNameForLogging() {
  return "SubresourceFilterSafeBrowsingActivationThrottle";
}

void SubresourceFilterSafeBrowsingActivationThrottle::OnCheckUrlResultOnUI(
    const SubresourceFilterSafeBrowsingClient::CheckResult& result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  size_t request_id = result.request_id;
  DCHECK_LT(request_id, check_results_.size());

  auto& stored_result = check_results_.at(request_id);
  DCHECK(!stored_result.finished);
  stored_result = result;
  if (!defer_time_.is_null() && request_id == check_results_.size() - 1) {
    NotifyResult();
    navigation_handle()->Resume();
  }
}

void SubresourceFilterSafeBrowsingActivationThrottle::CheckCurrentUrl() {
  check_results_.emplace_back();
  size_t id = check_results_.size() - 1;
  io_task_runner_->PostTask(
      FROM_HERE, base::Bind(&SubresourceFilterSafeBrowsingClient::CheckUrlOnIO,
                            base::Unretained(database_client_.get()),
                            navigation_handle()->GetURL(), id));
}

void SubresourceFilterSafeBrowsingActivationThrottle::NotifyResult() {
  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  if (!web_contents)
    return;

  using subresource_filter::ContentSubresourceFilterDriverFactory;

  const SubresourceFilterSafeBrowsingClient::CheckResult& result =
      check_results_.back();
  ContentSubresourceFilterDriverFactory* driver_factory =
      ContentSubresourceFilterDriverFactory::FromWebContents(web_contents);
  DCHECK(driver_factory);

  driver_factory->OnMainResourceMatchedSafeBrowsingBlacklist(
      navigation_handle()->GetURL(), std::vector<GURL>(), result.threat_type,
      result.pattern_type);

  base::TimeDelta delay = defer_time_.is_null()
                              ? base::TimeDelta::FromMilliseconds(0)
                              : base::TimeTicks::Now() - defer_time_;
  UMA_HISTOGRAM_TIMES("SubresourceFilter.PageLoad.SafeBrowsingDelay", delay);

  // Log a histogram for the delay we would have introduced if the throttle only
  // speculatively checks URLs on WillStartRequest. This is only different from
  // the actual delay if there was at least one redirect.
  base::TimeDelta no_redirect_speculation_delay =
      check_results_.size() > 1 ? result.check_time : delay;
  UMA_HISTOGRAM_TIMES(
      "SubresourceFilter.PageLoad.SafeBrowsingDelay.NoRedirectSpeculation",
      no_redirect_speculation_delay);
}

}  //  namespace subresource_filter

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_SAFE_BROWSING_ACTIVATION_THROTTLE_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_SAFE_BROWSING_ACTIVATION_THROTTLE_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/safe_browsing_db/database_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_client.h"
#include "content/public/browser/navigation_throttle.h"

namespace subresource_filter {

// Navigation throttle responsible for activating subresource filtering on page
// loads that match the SUBRESOURCE_FILTER Safe Browsing list.
class SubresourceFilterSafeBrowsingActivationThrottle
    : public content::NavigationThrottle,
      public base::SupportsWeakPtr<
          SubresourceFilterSafeBrowsingActivationThrottle> {
 public:
  SubresourceFilterSafeBrowsingActivationThrottle(
      content::NavigationHandle* handle,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
          database_manager);

  ~SubresourceFilterSafeBrowsingActivationThrottle() override;

  // content::NavigationThrottle:
  content::NavigationThrottle::ThrottleCheckResult WillStartRequest() override;
  content::NavigationThrottle::ThrottleCheckResult WillRedirectRequest()
      override;
  content::NavigationThrottle::ThrottleCheckResult WillProcessResponse()
      override;
  const char* GetNameForLogging() override;

  void OnCheckUrlResultOnUI(
      const SubresourceFilterSafeBrowsingClient::CheckResult& result);

 private:
  void CheckCurrentUrl();
  void NotifyResult();
  std::vector<SubresourceFilterSafeBrowsingClient::CheckResult> check_results_;

  scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  std::unique_ptr<SubresourceFilterSafeBrowsingClient,
                  base::OnTaskRunnerDeleter>
      database_client_;

  // Set to TimeTicks::Now() when the navigation is deferred in
  // WillProcessResponse. If deferral was not necessary, will remain null.
  base::TimeTicks defer_time_;

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterSafeBrowsingActivationThrottle);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_SAFE_BROWSING_ACTIVATION_THROTTLE_H_

// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_SAFE_BROWSING_ACTIVATION_THROTTLE_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_SAFE_BROWSING_ACTIVATION_THROTTLE_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "components/safe_browsing_db/database_manager.h"
#include "content/public/browser/navigation_throttle.h"
#include "url/gurl.h"

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
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
          database_manager);

  ~SubresourceFilterSafeBrowsingActivationThrottle() override;

  // content::NavigationThrottle:
  content::NavigationThrottle::ThrottleCheckResult WillProcessResponse()
      override;
  const char* GetNameForLogging() override;

  void OnCheckUrlResultOnUI(const GURL& url,
                            safe_browsing::SBThreatType threat_type,
                            safe_browsing::ThreatPatternType pattern_type);

 private:
  class SBDatabaseClient;

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  std::unique_ptr<SBDatabaseClient, base::OnTaskRunnerDeleter> database_client_;

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterSafeBrowsingActivationThrottle);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_SAFE_BROWSING_ACTIVATION_THROTTLE_H_

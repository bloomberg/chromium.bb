// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_activation_throttle.h"

#include <vector>

#include "base/timer/timer.h"
#include "components/safe_browsing_db/v4_local_database_manager.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_driver_factory.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace {

// Maximum time in milliseconds to wait for the Safe Browsing service to
// verify a URL. After this amount of time the outstanding check will be
// aborted, and the URL will be treated as if it didn't belong to the
// Subresource Filter only list.
constexpr base::TimeDelta kCheckURLTimeout = base::TimeDelta::FromSeconds(5);

}  // namespace

namespace subresource_filter {

class SubresourceFilterSafeBrowsingActivationThrottle::SBDatabaseClient
    : public safe_browsing::SafeBrowsingDatabaseManager::Client {
 public:
  SBDatabaseClient(
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
          database_manager,
      base::WeakPtr<SubresourceFilterSafeBrowsingActivationThrottle> throttle,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
      : database_manager_(std::move(database_manager)),
        throttle_(throttle),
        io_task_runner_(io_task_runner) {}

  ~SBDatabaseClient() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    database_manager_->CancelCheck(this);
  }

  void CheckUrlOnIO(const GURL& url) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    DCHECK(!url.is_empty());
    url_being_checked_ = url;
    if (database_manager_->CheckUrlForSubresourceFilter(url, this)) {
      OnCheckBrowseUrlResult(url, safe_browsing::SB_THREAT_TYPE_SAFE,
                             safe_browsing::ThreatMetadata());
      return;
    }
    timer_.Start(FROM_HERE, kCheckURLTimeout, this,
                 &SubresourceFilterSafeBrowsingActivationThrottle::
                     SBDatabaseClient::OnCheckUrlTimeout);
  }

  void OnCheckBrowseUrlResult(
      const GURL& url,
      safe_browsing::SBThreatType threat_type,
      const safe_browsing::ThreatMetadata& metadata) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    DCHECK_EQ(url_being_checked_, url);
    timer_.Stop();  // Cancel the timeout timer.
    io_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&SubresourceFilterSafeBrowsingActivationThrottle::
                       OnCheckUrlResultOnUI,
                   throttle_, url, threat_type, metadata.threat_pattern_type));
  }

  // Callback for when the safe browsing check has taken longer than
  // kCheckURLTimeout.
  void OnCheckUrlTimeout() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    database_manager_->CancelCheck(this);

    OnCheckBrowseUrlResult(url_being_checked_,
                           safe_browsing::SB_THREAT_TYPE_SAFE,
                           safe_browsing::ThreatMetadata());
  }

 private:
  scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager_;

  // Timer to abort the safe browsing check if it takes too long.
  base::OneShotTimer timer_;
  GURL url_being_checked_;

  base::WeakPtr<SubresourceFilterSafeBrowsingActivationThrottle> throttle_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(SBDatabaseClient);
};

SubresourceFilterSafeBrowsingActivationThrottle::
    SubresourceFilterSafeBrowsingActivationThrottle(
        content::NavigationHandle* handle,
        scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
            database_manager)
    : NavigationThrottle(handle),
      io_task_runner_(content::BrowserThread::GetTaskRunnerForThread(
          content::BrowserThread::IO)),
      database_client_(
          new SubresourceFilterSafeBrowsingActivationThrottle::SBDatabaseClient(
              std::move(database_manager),
              AsWeakPtr(),
              base::ThreadTaskRunnerHandle::Get()),
          base::OnTaskRunnerDeleter(io_task_runner_)) {}

SubresourceFilterSafeBrowsingActivationThrottle::
    ~SubresourceFilterSafeBrowsingActivationThrottle() {}

content::NavigationThrottle::ThrottleCheckResult
SubresourceFilterSafeBrowsingActivationThrottle::WillProcessResponse() {
  io_task_runner_->PostTask(
      FROM_HERE, base::Bind(&SubresourceFilterSafeBrowsingActivationThrottle::
                                SBDatabaseClient::CheckUrlOnIO,
                            base::Unretained(database_client_.get()),
                            navigation_handle()->GetURL()));
  return content::NavigationThrottle::ThrottleCheckResult::DEFER;
}

const char*
SubresourceFilterSafeBrowsingActivationThrottle::GetNameForLogging() {
  return "SubresourceFilterSafeBrowsingActivationThrottle";
}

void SubresourceFilterSafeBrowsingActivationThrottle::OnCheckUrlResultOnUI(
    const GURL& url,
    safe_browsing::SBThreatType threat_type,
    safe_browsing::ThreatPatternType pattern_type) {
  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  if (web_contents) {
    using subresource_filter::ContentSubresourceFilterDriverFactory;
    ContentSubresourceFilterDriverFactory* driver_factory =
        ContentSubresourceFilterDriverFactory::FromWebContents(web_contents);
    DCHECK(driver_factory);

    driver_factory->OnMainResourceMatchedSafeBrowsingBlacklist(
        url, std::vector<GURL>(), threat_type, pattern_type);
  }
  // TODO(https://crbug.com/704508): We should measure the delay introduces by
  // this check. Similarly, as it's done the Safe Browsing Resource throttle.
  navigation_handle()->Resume();
}

}  //  namespace subresource_filter

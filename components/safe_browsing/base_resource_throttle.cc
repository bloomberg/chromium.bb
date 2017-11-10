// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/base_resource_throttle.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "components/safe_browsing/base_ui_manager.h"
#include "components/safe_browsing/common/utils.h"
#include "components/safe_browsing/db/util.h"
#include "components/safe_browsing/web_ui/constants.h"
#include "components/security_interstitials/content/unsafe_resource.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "net/base/load_flags.h"
#include "net/log/net_log_event_type.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request.h"

using net::NetLogEventType;

namespace safe_browsing {

namespace {

// Maximum time in milliseconds to wait for the safe browsing service to
// verify a URL. After this amount of time the outstanding check will be
// aborted, and the URL will be treated as if it were safe.
const int kCheckUrlTimeoutMs = 5000;

}  // namespace

// TODO(eroman): Downgrade these CHECK()s to DCHECKs once there is more
//               unit test coverage.

BaseResourceThrottle::BaseResourceThrottle(
    const net::URLRequest* request,
    content::ResourceType resource_type,
    SBThreatTypeSet threat_types,
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    scoped_refptr<BaseUIManager> ui_manager)
    : ui_manager_(ui_manager),
      threat_type_(SB_THREAT_TYPE_SAFE),
      threat_types_(std::move(threat_types)),
      database_manager_(database_manager),
      request_(request),
      state_(STATE_NONE),
      defer_state_(DEFERRED_NONE),
      resource_type_(resource_type),
      net_event_logger_(&request->net_log()) {}

BaseResourceThrottle::~BaseResourceThrottle() {
  if (defer_state_ != DEFERRED_NONE) {
    net_event_logger_.EndNetLogEvent(NetLogEventType::SAFE_BROWSING_DEFERRED,
                                     nullptr, nullptr);
  }

  if (state_ == STATE_CHECKING_URL) {
    database_manager_->CancelCheck(this);
    net_event_logger_.EndNetLogEvent(
        NetLogEventType::SAFE_BROWSING_CHECKING_URL, "result",
        "request_canceled");
  }

  if (!user_action_involved_)
    LogNoUserActionResourceLoadingDelay(total_delay_);
}

void BaseResourceThrottle::WillStartRequest(bool* defer) {
  // We need to check the new URL before starting the request.
  if (CheckUrl(request_->url()))
    return;

  // We let the check run in parallel with resource load only if this
  // db_manager only supports asynchronous checks, like on mobile.
  // Otherwise, we defer now.
  if (database_manager_->ChecksAreAlwaysAsync())
    return;

  // If the URL couldn't be verified synchronously, defer starting the
  // request until the check has completed.
  defer_state_ = DEFERRED_START;
  defer_start_time_ = base::TimeTicks::Now();
  *defer = true;
  net_event_logger_.BeginNetLogEvent(NetLogEventType::SAFE_BROWSING_DEFERRED,
                                     request_->url(), "defer_reason",
                                     "at_start");
}

void BaseResourceThrottle::WillProcessResponse(bool* defer) {
  CHECK_EQ(defer_state_, DEFERRED_NONE);
  // TODO(nparker): Maybe remove this check, since it should have no effect.
  if (!database_manager_->ChecksAreAlwaysAsync())
    return;

  if (state_ == STATE_CHECKING_URL ||
      state_ == STATE_DISPLAYING_BLOCKING_PAGE) {
    defer_state_ = DEFERRED_PROCESSING;
    defer_start_time_ = base::TimeTicks::Now();
    *defer = true;
    net_event_logger_.BeginNetLogEvent(NetLogEventType::SAFE_BROWSING_DEFERRED,
                                       request_->url(), "defer_reason",
                                       "at_response");
  }
}

bool BaseResourceThrottle::MustProcessResponseBeforeReadingBody() {
  // On Android, SafeBrowsing may only decide to cancel the request when the
  // response has been received. Therefore, no part of it should be cached
  // until this ResourceThrottle has been able to check the response. This
  // prevents the following scenario:
  //   1) A request is made for foo.com which has been hacked.
  //   2) The request is only canceled at WillProcessResponse stage, but part of
  //   it has been cached.
  //   3) foo.com is no longer hacked and removed from the SafeBrowsing list.
  //   4) The user requests foo.com, which is not on the SafeBrowsing list. This
  //   is deemed safe. However, the resource is actually served from cache,
  //   using the version that was previously stored.
  //   5) This results in the  user accessing an unsafe resource without being
  //   notified that it's dangerous.
  // TODO(clamy): Add a browser test that checks this specific scenario.
  return true;
}

void BaseResourceThrottle::WillRedirectRequest(
    const net::RedirectInfo& redirect_info,
    bool* defer) {
  CHECK_EQ(defer_state_, DEFERRED_NONE);

  // Prev check completed and was safe.
  if (state_ == STATE_NONE) {
    // Save the redirect urls for possible malware detail reporting later.
    redirect_urls_.push_back(redirect_info.new_url);

    // We need to check the new URL before following the redirect.
    if (CheckUrl(redirect_info.new_url))
      return;
    defer_state_ = DEFERRED_REDIRECT;
  } else {
    CHECK(state_ == STATE_CHECKING_URL ||
          state_ == STATE_DISPLAYING_BLOCKING_PAGE);
    // We can't check this new URL until we have finished checking
    // the prev one, or resumed from the blocking page.
    unchecked_redirect_url_ = redirect_info.new_url;
    defer_state_ = DEFERRED_UNCHECKED_REDIRECT;
  }

  defer_start_time_ = base::TimeTicks::Now();
  *defer = true;
  net_event_logger_.BeginNetLogEvent(
      NetLogEventType::SAFE_BROWSING_DEFERRED, redirect_info.new_url,
      "defer_reason",
      defer_state_ == DEFERRED_REDIRECT ? "redirect" : "unchecked_redirect");
}

const char* BaseResourceThrottle::GetNameForLogging() const {
  return "BaseResourceThrottle";
}

void BaseResourceThrottle::MaybeDestroyPrerenderContents(
    const content::ResourceRequestInfo* info) {}

// SafeBrowsingService::Client implementation, called on the IO thread once
// the URL has been classified.
void BaseResourceThrottle::OnCheckBrowseUrlResult(
    const GURL& url,
    SBThreatType threat_type,
    const ThreatMetadata& metadata) {
  CHECK_EQ(state_, STATE_CHECKING_URL);
  CHECK(url.is_valid());
  CHECK(url_being_checked_.is_valid());
  CHECK_EQ(url, url_being_checked_);

  timer_.Stop();  // Cancel the timeout timer.
  threat_type_ = threat_type;
  state_ = STATE_NONE;

  if (defer_state_ != DEFERRED_NONE) {
    total_delay_ += base::TimeTicks::Now() - defer_start_time_;
    net_event_logger_.EndNetLogEvent(NetLogEventType::SAFE_BROWSING_DEFERRED,
                                     nullptr, nullptr);
  }
  net_event_logger_.EndNetLogEvent(
      NetLogEventType::SAFE_BROWSING_CHECKING_URL, "result",
      threat_type_ == SB_THREAT_TYPE_SAFE ? "safe" : "unsafe");

  if (threat_type == SB_THREAT_TYPE_SAFE) {
    if (defer_state_ != DEFERRED_NONE) {
      // Log how much time the safe browsing check cost us.
      LogDelay(base::TimeTicks::Now() - defer_start_time_);
      ResumeRequest();
    } else {
      LogDelay(base::TimeDelta());
    }
    return;
  }

  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request_);

  if (request_->load_flags() & net::LOAD_PREFETCH) {
    // Destroy the prefetch with FINAL_STATUS_SAFEBROSWING.
    if (resource_type_ == content::RESOURCE_TYPE_MAIN_FRAME) {
      MaybeDestroyPrerenderContents(info);
    }
    // Don't prefetch resources that fail safe browsing, disallow them.
    Cancel();
    UMA_HISTOGRAM_ENUMERATION("SB2.ResourceTypes2.UnsafePrefetchCanceled",
                              resource_type_, content::RESOURCE_TYPE_LAST_TYPE);
    return;
  }

  UMA_HISTOGRAM_ENUMERATION("SB2.ResourceTypes2.Unsafe", resource_type_,
                            content::RESOURCE_TYPE_LAST_TYPE);

  user_action_involved_ = true;

  security_interstitials::UnsafeResource resource;
  resource.url = url;
  resource.original_url = request_->original_url();
  resource.redirect_urls = redirect_urls_;
  resource.is_subresource = resource_type_ != content::RESOURCE_TYPE_MAIN_FRAME;
  resource.is_subframe = resource_type_ == content::RESOURCE_TYPE_SUB_FRAME;
  resource.threat_type = threat_type;
  resource.threat_metadata = metadata;
  resource.callback = base::Bind(
      &BaseResourceThrottle::OnBlockingPageComplete, AsWeakPtr());
  resource.callback_thread = content::BrowserThread::GetTaskRunnerForThread(
      content::BrowserThread::IO);
  resource.web_contents_getter = info->GetWebContentsGetterForRequest();
  resource.threat_source = database_manager_->GetThreatSource();

  state_ = STATE_DISPLAYING_BLOCKING_PAGE;

  StartDisplayingBlockingPageHelper(resource);
}

void BaseResourceThrottle::OnBlockingPageComplete(bool proceed) {
  CHECK_EQ(state_, STATE_DISPLAYING_BLOCKING_PAGE);
  state_ = STATE_NONE;

  if (proceed) {
    threat_type_ = SB_THREAT_TYPE_SAFE;
    if (defer_state_ != DEFERRED_NONE) {
      ResumeRequest();
    }
  } else {
    CancelResourceLoad();
  }
}

void BaseResourceThrottle::CancelResourceLoad() {
  Cancel();
}

scoped_refptr<BaseUIManager> BaseResourceThrottle::ui_manager() {
  return ui_manager_;
}

bool BaseResourceThrottle::CheckUrl(const GURL& url) {
  TRACE_EVENT1("loader", "BaseResourceThrottle::CheckUrl", "url",
               url.spec());
  CHECK_EQ(state_, STATE_NONE);
  // To reduce aggregate latency on mobile, check only the most dangerous
  // resource types.
  if (!database_manager_->CanCheckResourceType(resource_type_)) {
    // TODO(vakh): Consider changing this metric to SafeBrowsing.V4ResourceType
    // to be consistent with the other PVer4 metrics.
    UMA_HISTOGRAM_ENUMERATION("SB2.ResourceTypes2.Skipped", resource_type_,
                              content::RESOURCE_TYPE_LAST_TYPE);
    return true;
  }

  // TODO(vakh): Consider changing this metric to SafeBrowsing.V4ResourceType to
  // be consistent with the other PVer4 metrics.
  UMA_HISTOGRAM_ENUMERATION("SB2.ResourceTypes2.Checked", resource_type_,
                            content::RESOURCE_TYPE_LAST_TYPE);

  if (CheckWebUIUrls(url)) {
    return false;
  }

  if (database_manager_->CheckBrowseUrl(url, threat_types_, this)) {
    threat_type_ = SB_THREAT_TYPE_SAFE;
    LogDelay(base::TimeDelta());  // No delay.
    return true;
  }

  state_ = STATE_CHECKING_URL;
  url_being_checked_ = url;
  // Note on net_log calls: Synchronous checks are not logged at all.
  net_event_logger_.BeginNetLogEvent(
      NetLogEventType::SAFE_BROWSING_CHECKING_URL, url, nullptr, nullptr);

  // Start a timer to abort the check if it takes too long.
  // TODO(nparker): Set this only when we defer, based on remaining time,
  // so we don't cancel earlier than necessary.
  timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(kCheckUrlTimeoutMs),
               this, &BaseResourceThrottle::OnCheckUrlTimeout);

  return false;
}

bool BaseResourceThrottle::CheckWebUIUrls(const GURL& url) {
  DCHECK(threat_type_ == safe_browsing::SB_THREAT_TYPE_SAFE);
  if (url == kChromeUISafeBrowsingMatchMalwareUrl) {
    threat_type_ = safe_browsing::SB_THREAT_TYPE_URL_MALWARE;
  } else if (url == kChromeUISafeBrowsingMatchPhishingUrl) {
    threat_type_ = safe_browsing::SB_THREAT_TYPE_URL_PHISHING;
  } else if (url == kChromeUISafeBrowsingMatchUnwantedUrl) {
    threat_type_ = safe_browsing::SB_THREAT_TYPE_URL_UNWANTED;
  }

  if (threat_type_ != safe_browsing::SB_THREAT_TYPE_SAFE) {
    state_ = STATE_CHECKING_URL;
    url_being_checked_ = url;
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&BaseResourceThrottle::OnCheckBrowseUrlResult, AsWeakPtr(),
                   url, threat_type_, ThreatMetadata()));
    return true;
  }
  return false;
}

void BaseResourceThrottle::OnCheckUrlTimeout() {
  CHECK_EQ(state_, STATE_CHECKING_URL);

  database_manager_->CancelCheck(this);

  OnCheckBrowseUrlResult(url_being_checked_, safe_browsing::SB_THREAT_TYPE_SAFE,
                         ThreatMetadata());
}

void BaseResourceThrottle::ResumeRequest() {
  CHECK_EQ(state_, STATE_NONE);
  CHECK_NE(defer_state_, DEFERRED_NONE);

  bool resume = true;
  if (defer_state_ == DEFERRED_UNCHECKED_REDIRECT) {
    // Save the redirect urls for possible malware detail reporting later.
    redirect_urls_.push_back(unchecked_redirect_url_);
    if (!CheckUrl(unchecked_redirect_url_)) {
      // We're now waiting for the unchecked_redirect_url_.
      defer_state_ = DEFERRED_REDIRECT;
      defer_start_time_ = base::TimeTicks::Now();
      resume = false;
      net_event_logger_.BeginNetLogEvent(
          NetLogEventType::SAFE_BROWSING_DEFERRED, unchecked_redirect_url_,
          "defer_reason", "resumed_redirect");
    }
  }

  if (resume) {
    defer_state_ = DEFERRED_NONE;
    Resume();
  }
}

}  // namespace safe_browsing

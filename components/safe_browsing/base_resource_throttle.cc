// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/base_resource_throttle.h"

#include <iterator>
#include <utility>

#include "base/debug/alias.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "components/safe_browsing/base_ui_manager.h"
#include "components/safe_browsing_db/util.h"
#include "components/security_interstitials/content/unsafe_resource.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_driver_factory.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "net/base/load_flags.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_source_type.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request.h"

using net::NetLogEventType;
using net::NetLogSourceType;

namespace safe_browsing {

namespace {

// Maximum time in milliseconds to wait for the safe browsing service to
// verify a URL. After this amount of time the outstanding check will be
// aborted, and the URL will be treated as if it were safe.
const int kCheckUrlTimeoutMs = 5000;

// Return a dictionary with "url"=|url-spec| and optionally
// |name|=|value| (if not null), for netlogging.
// This will also add a reference to the original request's net_log ID.
std::unique_ptr<base::Value> NetLogUrlCallback(
    const net::URLRequest* request,
    const GURL& url,
    const char* name,
    const char* value,
    net::NetLogCaptureMode /* capture_mode */) {
  std::unique_ptr<base::DictionaryValue> event_params(
      new base::DictionaryValue());
  event_params->SetString("url", url.spec());
  if (name && value)
    event_params->SetString(name, value);
  request->net_log().source().AddToEventParameters(event_params.get());
  return std::move(event_params);
}

// Return a dictionary with |name|=|value|, for netlogging.
std::unique_ptr<base::Value> NetLogStringCallback(const char* name,
                                                  const char* value,
                                                  net::NetLogCaptureMode) {
  std::unique_ptr<base::DictionaryValue> event_params(
      new base::DictionaryValue());
  if (name && value)
    event_params->SetString(name, value);
  return std::move(event_params);
}

}  // namespace

// TODO(eroman): Downgrade these CHECK()s to DCHECKs once there is more
//               unit test coverage.

BaseResourceThrottle::BaseResourceThrottle(
    const net::URLRequest* request,
    content::ResourceType resource_type,
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    scoped_refptr<BaseUIManager> ui_manager)
    : ui_manager_(ui_manager),
      threat_type_(SB_THREAT_TYPE_SAFE),
      database_manager_(database_manager),
      request_(request),
      state_(STATE_NONE),
      defer_state_(DEFERRED_NONE),
      resource_type_(resource_type),
      net_log_with_source_(
          net::NetLogWithSource::Make(request->net_log().net_log(),
                                      NetLogSourceType::SAFE_BROWSING)) {}

// static
BaseResourceThrottle* BaseResourceThrottle::MaybeCreate(
    net::URLRequest* request,
    content::ResourceType resource_type,
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    scoped_refptr<BaseUIManager> ui_manager) {
  if (database_manager->IsSupported()) {
    return new BaseResourceThrottle(request, resource_type,
                                    database_manager, ui_manager);
  }
  return nullptr;
}

BaseResourceThrottle::~BaseResourceThrottle() {
  if (defer_state_ != DEFERRED_NONE) {
    EndNetLogEvent(NetLogEventType::SAFE_BROWSING_DEFERRED, nullptr, nullptr);
  }

  if (state_ == STATE_CHECKING_URL) {
    database_manager_->CancelCheck(this);
    EndNetLogEvent(NetLogEventType::SAFE_BROWSING_CHECKING_URL, "result",
                   "request_canceled");
  }
}

// Note on net_log calls: SAFE_BROWSING_DEFERRED events must be wholly
// nested within SAFE_BROWSING_CHECKING_URL events.  Synchronous checks
// are not logged at all.
void BaseResourceThrottle::BeginNetLogEvent(NetLogEventType type,
                                            const GURL& url,
                                            const char* name,
                                            const char* value) {
  net_log_with_source_.BeginEvent(
      type, base::Bind(&NetLogUrlCallback, request_, url, name, value));
  request_->net_log().AddEvent(
      type, net_log_with_source_.source().ToEventParametersCallback());
}

void BaseResourceThrottle::EndNetLogEvent(NetLogEventType type,
                                          const char* name,
                                          const char* value) {
  net_log_with_source_.EndEvent(type,
                                base::Bind(&NetLogStringCallback, name, value));
  request_->net_log().AddEvent(
      type, net_log_with_source_.source().ToEventParametersCallback());
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
  BeginNetLogEvent(NetLogEventType::SAFE_BROWSING_DEFERRED, request_->url(),
                   "defer_reason", "at_start");
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
    BeginNetLogEvent(NetLogEventType::SAFE_BROWSING_DEFERRED, request_->url(),
                     "defer_reason", "at_response");
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
  BeginNetLogEvent(
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
  // TODO(vakh): The following base::debug::Alias() and CHECK calls should be
  // removed after http://crbug.com/660293 is fixed.
  CHECK(url.is_valid());
  CHECK(url_being_checked_.is_valid());
  if (url != url_being_checked_) {
    bool url_had_timed_out = timed_out_urls_.count(url) > 0;
    char buf[1000];
    snprintf(buf, sizeof(buf), "sbtr::ocbur:%d:%s -- %s\n", url_had_timed_out,
             url.spec().c_str(), url_being_checked_.spec().c_str());
    base::debug::Alias(buf);
    CHECK(false) << "buf: " << buf;
  }

  timer_.Stop();  // Cancel the timeout timer.
  threat_type_ = threat_type;
  state_ = STATE_NONE;

  if (defer_state_ != DEFERRED_NONE) {
    EndNetLogEvent(NetLogEventType::SAFE_BROWSING_DEFERRED, nullptr, nullptr);
  }
  EndNetLogEvent(
      NetLogEventType::SAFE_BROWSING_CHECKING_URL, "result",
      threat_type_ == SB_THREAT_TYPE_SAFE ? "safe" : "unsafe");

  if (threat_type == SB_THREAT_TYPE_SAFE) {
    if (defer_state_ != DEFERRED_NONE) {
      // Log how much time the safe browsing check cost us.
      ui_manager_->LogPauseDelay(base::TimeTicks::Now() - defer_start_time_);
      ResumeRequest();
    } else {
      ui_manager_->LogPauseDelay(base::TimeDelta());
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

void BaseResourceThrottle::StartDisplayingBlockingPageHelper(
    security_interstitials::UnsafeResource resource) {
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&BaseResourceThrottle::StartDisplayingBlockingPage,
                 AsWeakPtr(), ui_manager_, resource));
}

// Static
void BaseResourceThrottle::NotifySubresourceFilterOfBlockedResource(
    const security_interstitials::UnsafeResource& resource) {
  content::WebContents* web_contents = resource.web_contents_getter.Run();
  DCHECK(web_contents);
  // Once activated, the subresource filter will filter subresources, but is
  // triggered when the main frame document matches Safe Browsing blacklists.
  if (!resource.is_subresource) {
    using subresource_filter::ContentSubresourceFilterDriverFactory;
    ContentSubresourceFilterDriverFactory* driver_factory =
        ContentSubresourceFilterDriverFactory::FromWebContents(web_contents);

    // Content embedders (such as Android Webview) do not have a driver_factory.
    if (driver_factory) {
      // For a redirect chain of A -> B -> C, the subresource filter expects C
      // as the resource URL and [A, B] as redirect URLs.
      std::vector<GURL> redirect_parent_urls;
      if (!resource.redirect_urls.empty()) {
        redirect_parent_urls.push_back(resource.original_url);
        redirect_parent_urls.insert(redirect_parent_urls.end(),
                                    resource.redirect_urls.begin(),
                                    std::prev(resource.redirect_urls.end()));
      }
      driver_factory->OnMainResourceMatchedSafeBrowsingBlacklist(
          resource.url, redirect_parent_urls, resource.threat_type,
          resource.threat_metadata.threat_pattern_type);
    }
  }
}

// Static
void BaseResourceThrottle::StartDisplayingBlockingPage(
    const base::WeakPtr<BaseResourceThrottle>& throttle,
    scoped_refptr<BaseUIManager> ui_manager,
    const security_interstitials::UnsafeResource& resource) {
  content::WebContents* web_contents = resource.web_contents_getter.Run();
  if (web_contents) {
    NotifySubresourceFilterOfBlockedResource(resource);
    ui_manager->DisplayBlockingPage(resource);
    return;
  }

  // Tab is gone or it's being prerendered.
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&BaseResourceThrottle::Cancel, throttle));
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

  if (database_manager_->CheckBrowseUrl(url, this)) {
    threat_type_ = SB_THREAT_TYPE_SAFE;
    ui_manager_->LogPauseDelay(base::TimeDelta());  // No delay.
    return true;
  }

  state_ = STATE_CHECKING_URL;
  url_being_checked_ = url;
  BeginNetLogEvent(NetLogEventType::SAFE_BROWSING_CHECKING_URL, url, nullptr,
                   nullptr);

  // Start a timer to abort the check if it takes too long.
  // TODO(nparker): Set this only when we defer, based on remaining time,
  // so we don't cancel earlier than necessary.
  timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(kCheckUrlTimeoutMs),
               this, &BaseResourceThrottle::OnCheckUrlTimeout);

  return false;
}

void BaseResourceThrottle::OnCheckUrlTimeout() {
  CHECK_EQ(state_, STATE_CHECKING_URL);

  database_manager_->CancelCheck(this);

  OnCheckBrowseUrlResult(url_being_checked_, safe_browsing::SB_THREAT_TYPE_SAFE,
                         ThreatMetadata());

  timed_out_urls_.insert(url_being_checked_);
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
      resume = false;
      BeginNetLogEvent(NetLogEventType::SAFE_BROWSING_DEFERRED,
                       unchecked_redirect_url_, "defer_reason",
                       "resumed_redirect");
    }
  }

  if (resume) {
    defer_state_ = DEFERRED_NONE;
    Resume();
  }
}

}  // namespace safe_browsing

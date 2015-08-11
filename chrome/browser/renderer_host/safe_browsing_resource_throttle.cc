// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/safe_browsing_resource_throttle.h"

#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "net/base/load_flags.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request.h"

// Maximum time in milliseconds to wait for the safe browsing service to
// verify a URL. After this amount of time the outstanding check will be
// aborted, and the URL will be treated as if it were safe.
static const int kCheckUrlTimeoutMs = 5000;

// TODO(eroman): Downgrade these CHECK()s to DCHECKs once there is more
//               unit test coverage.

SafeBrowsingResourceThrottle::SafeBrowsingResourceThrottle(
    const net::URLRequest* request,
    content::ResourceType resource_type,
    SafeBrowsingService* safe_browsing,
    bool defer_at_start)
    : defer_at_start_(defer_at_start),
      state_(STATE_NONE),
      defer_state_(DEFERRED_NONE),
      threat_type_(SB_THREAT_TYPE_SAFE),
      database_manager_(safe_browsing->database_manager()),
      ui_manager_(safe_browsing->ui_manager()),
      request_(request),
      is_subresource_(resource_type != content::RESOURCE_TYPE_MAIN_FRAME),
      is_subframe_(resource_type == content::RESOURCE_TYPE_SUB_FRAME) {
}

SafeBrowsingResourceThrottle::~SafeBrowsingResourceThrottle() {
  if (state_ == STATE_CHECKING_URL)
    database_manager_->CancelCheck(this);
}

void SafeBrowsingResourceThrottle::WillStartRequest(bool* defer) {
  // We need to check the new URL before starting the request.
  if (CheckUrl(request_->url()))
    return;

  if (!defer_at_start_)
    return;

  // If the URL couldn't be verified synchronously, defer starting the
  // request until the check has completed.
  defer_state_ = DEFERRED_START;
  defer_start_time_ = base::TimeTicks::Now();
  *defer = true;
}

void SafeBrowsingResourceThrottle::WillProcessResponse(bool* defer) {
  CHECK_EQ(defer_state_, DEFERRED_NONE);
  if (defer_at_start_)
    return;

  if (state_ == STATE_CHECKING_URL ||
      state_ == STATE_DISPLAYING_BLOCKING_PAGE) {
    defer_state_ = DEFERRED_PROCESSING;
    defer_start_time_ = base::TimeTicks::Now();
    *defer = true;
  }
}

void SafeBrowsingResourceThrottle::WillRedirectRequest(
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
}

const char* SafeBrowsingResourceThrottle::GetNameForLogging() const {
  return "SafeBrowsingResourceThrottle";
}

// SafeBrowsingService::Client implementation, called on the IO thread once
// the URL has been classified.
void SafeBrowsingResourceThrottle::OnCheckBrowseUrlResult(
    const GURL& url,
    SBThreatType threat_type,
    const std::string& metadata) {
  CHECK_EQ(state_, STATE_CHECKING_URL);
  CHECK_EQ(url, url_being_checked_);

  timer_.Stop();  // Cancel the timeout timer.
  threat_type_ = threat_type;
  state_ = STATE_NONE;

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

  if (request_->load_flags() & net::LOAD_PREFETCH) {
    // Don't prefetch resources that fail safe browsing, disallow them.
    controller()->Cancel();
    return;
  }

  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request_);

  SafeBrowsingUIManager::UnsafeResource resource;
  resource.url = url;
  resource.original_url = request_->original_url();
  resource.redirect_urls = redirect_urls_;
  resource.is_subresource = is_subresource_;
  resource.is_subframe = is_subframe_;
  resource.threat_type = threat_type;
  resource.threat_metadata = metadata;
  resource.callback = base::Bind(
      &SafeBrowsingResourceThrottle::OnBlockingPageComplete, AsWeakPtr());
  resource.render_process_host_id = info->GetChildID();
  resource.render_view_id =  info->GetRouteID();
  resource.threat_source = SafeBrowsingUIManager::FROM_DEVICE;

  state_ = STATE_DISPLAYING_BLOCKING_PAGE;

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&SafeBrowsingResourceThrottle::StartDisplayingBlockingPage,
                 AsWeakPtr(), ui_manager_, resource));
}

void SafeBrowsingResourceThrottle::StartDisplayingBlockingPage(
    const base::WeakPtr<SafeBrowsingResourceThrottle>& throttle,
    scoped_refptr<SafeBrowsingUIManager> ui_manager,
    const SafeBrowsingUIManager::UnsafeResource& resource) {
  content::RenderViewHost* rvh = content::RenderViewHost::FromID(
      resource.render_process_host_id, resource.render_view_id);
  if (rvh) {
    content::WebContents* web_contents =
        content::WebContents::FromRenderViewHost(rvh);
    prerender::PrerenderContents* prerender_contents =
        prerender::PrerenderContents::FromWebContents(web_contents);

    if (prerender_contents) {
      prerender_contents->Destroy(prerender::FINAL_STATUS_SAFE_BROWSING);
    } else {
      ui_manager->DisplayBlockingPage(resource);
      return;
    }
  }

  // Tab is gone or it's being prerendered.
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&SafeBrowsingResourceThrottle::Cancel, throttle));
}

void SafeBrowsingResourceThrottle::Cancel() {
  controller()->Cancel();
}

// SafeBrowsingService::UrlCheckCallback implementation, called on the IO
// thread when the user has decided to proceed with the current request, or
// go back.
void SafeBrowsingResourceThrottle::OnBlockingPageComplete(bool proceed) {
  CHECK_EQ(state_, STATE_DISPLAYING_BLOCKING_PAGE);
  state_ = STATE_NONE;

  if (proceed) {
    threat_type_ = SB_THREAT_TYPE_SAFE;
    if (defer_state_ != DEFERRED_NONE) {
      ResumeRequest();
    }
  } else {
    controller()->Cancel();
  }
}

bool SafeBrowsingResourceThrottle::CheckUrl(const GURL& url) {
  CHECK_EQ(state_, STATE_NONE);
  bool succeeded_synchronously = database_manager_->CheckBrowseUrl(url, this);
  if (succeeded_synchronously) {
    threat_type_ = SB_THREAT_TYPE_SAFE;
    ui_manager_->LogPauseDelay(base::TimeDelta());  // No delay.
    return true;
  }

  state_ = STATE_CHECKING_URL;
  url_being_checked_ = url;

  // Start a timer to abort the check if it takes too long.
  // TODO(nparker): Set this only when we defer, based on remaining time,
  // so we don't cancel earlier than necessary.
  timer_.Start(FROM_HERE,
               base::TimeDelta::FromMilliseconds(kCheckUrlTimeoutMs),
               this, &SafeBrowsingResourceThrottle::OnCheckUrlTimeout);

  return false;
}

void SafeBrowsingResourceThrottle::OnCheckUrlTimeout() {
  CHECK_EQ(state_, STATE_CHECKING_URL);

  database_manager_->CancelCheck(this);
  OnCheckBrowseUrlResult(
      url_being_checked_, SB_THREAT_TYPE_SAFE, std::string());
}

void SafeBrowsingResourceThrottle::ResumeRequest() {
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
    }
  }

  if (resume) {
    defer_state_ = DEFERRED_NONE;
    controller()->Resume();
  }
}

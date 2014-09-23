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
    SafeBrowsingService* safe_browsing)
    : state_(STATE_NONE),
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

  // If the URL couldn't be verified synchronously, defer starting the
  // request until the check has completed.
  defer_state_ = DEFERRED_START;
  *defer = true;
}

void SafeBrowsingResourceThrottle::WillRedirectRequest(const GURL& new_url,
                                                       bool* defer) {
  CHECK(state_ == STATE_NONE);
  CHECK(defer_state_ == DEFERRED_NONE);

  // Save the redirect urls for possible malware detail reporting later.
  redirect_urls_.push_back(new_url);

  // We need to check the new URL before following the redirect.
  if (CheckUrl(new_url))
    return;

  // If the URL couldn't be verified synchronously, defer following the
  // redirect until the SafeBrowsing check is complete. Store the redirect
  // context so we can pass it on to other handlers once we have completed
  // our check.
  defer_state_ = DEFERRED_REDIRECT;
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
  CHECK(state_ == STATE_CHECKING_URL);
  CHECK(defer_state_ != DEFERRED_NONE);
  CHECK(url == url_being_checked_) << "Was expecting: " << url_being_checked_
                                   << " but got: " << url;

#if defined(OS_ANDROID)
  // Temporarily disable SB interstitial during Finch experiment.
  // The database check is still exercised, but the interstitial never shown.
  threat_type = SB_THREAT_TYPE_SAFE;
#endif

  timer_.Stop();  // Cancel the timeout timer.
  threat_type_ = threat_type;
  state_ = STATE_NONE;

  if (threat_type == SB_THREAT_TYPE_SAFE) {
    // Log how much time the safe browsing check cost us.
    ui_manager_->LogPauseDelay(base::TimeTicks::Now() - url_check_start_time_);

    // Continue the request.
    ResumeRequest();
    return;
  }

  if (request_->load_flags() & net::LOAD_PREFETCH) {
    // Don't prefetch resources that fail safe browsing, disallow
    // them.
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
  bool should_show_blocking_page = true;

  content::RenderViewHost* rvh = content::RenderViewHost::FromID(
      resource.render_process_host_id, resource.render_view_id);
  if (rvh) {
    content::WebContents* web_contents =
        content::WebContents::FromRenderViewHost(rvh);
    prerender::PrerenderContents* prerender_contents =
        prerender::PrerenderContents::FromWebContents(web_contents);
    if (prerender_contents) {
      prerender_contents->Destroy(prerender::FINAL_STATUS_SAFE_BROWSING);
      should_show_blocking_page = false;
    }

    if (should_show_blocking_page)  {
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
  CHECK(state_ == STATE_DISPLAYING_BLOCKING_PAGE);
  state_ = STATE_NONE;

  if (proceed) {
    threat_type_ = SB_THREAT_TYPE_SAFE;
    ResumeRequest();
  } else {
    controller()->Cancel();
  }
}

bool SafeBrowsingResourceThrottle::CheckUrl(const GURL& url) {
  CHECK(state_ == STATE_NONE);
  bool succeeded_synchronously = database_manager_->CheckBrowseUrl(url, this);
  if (succeeded_synchronously) {
    threat_type_ = SB_THREAT_TYPE_SAFE;
    ui_manager_->LogPauseDelay(base::TimeDelta());  // No delay.
    return true;
  }

  state_ = STATE_CHECKING_URL;
  url_being_checked_ = url;

  // Record the start time of the check.
  url_check_start_time_ = base::TimeTicks::Now();

  // Start a timer to abort the check if it takes too long.
  timer_.Start(FROM_HERE,
               base::TimeDelta::FromMilliseconds(kCheckUrlTimeoutMs),
               this, &SafeBrowsingResourceThrottle::OnCheckUrlTimeout);

  return false;
}

void SafeBrowsingResourceThrottle::OnCheckUrlTimeout() {
  CHECK(state_ == STATE_CHECKING_URL);
  CHECK(defer_state_ != DEFERRED_NONE);

  database_manager_->CancelCheck(this);
  OnCheckBrowseUrlResult(
      url_being_checked_, SB_THREAT_TYPE_SAFE, std::string());
}

void SafeBrowsingResourceThrottle::ResumeRequest() {
  CHECK(state_ == STATE_NONE);
  CHECK(defer_state_ != DEFERRED_NONE);

  defer_state_ = DEFERRED_NONE;
  controller()->Resume();
}

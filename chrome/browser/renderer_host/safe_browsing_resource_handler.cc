// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/safe_browsing_resource_handler.h"

#include "base/logging.h"
#include "content/browser/renderer_host/global_request_id.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/browser/renderer_host/resource_message_filter.h"
#include "content/common/resource_response.h"
#include "net/base/net_errors.h"
#include "net/base/io_buffer.h"

// Maximum time in milliseconds to wait for the safe browsing service to
// verify a URL. After this amount of time the outstanding check will be
// aborted, and the URL will be treated as if it were safe.
static const int kCheckUrlTimeoutMs = 5000;

// TODO(eroman): Downgrade these CHECK()s to DCHECKs once there is more
//               unit test coverage.

SafeBrowsingResourceHandler::SafeBrowsingResourceHandler(
    ResourceHandler* handler,
    int render_process_host_id,
    int render_view_id,
    ResourceType::Type resource_type,
    SafeBrowsingService* safe_browsing,
    ResourceDispatcherHost* resource_dispatcher_host)
    : state_(STATE_NONE),
      defer_state_(DEFERRED_NONE),
      safe_browsing_result_(SafeBrowsingService::SAFE),
      deferred_request_id_(-1),
      next_handler_(handler),
      render_process_host_id_(render_process_host_id),
      render_view_id_(render_view_id),
      safe_browsing_(safe_browsing),
      rdh_(resource_dispatcher_host),
      resource_type_(resource_type) {
}

SafeBrowsingResourceHandler::~SafeBrowsingResourceHandler() {
}

bool SafeBrowsingResourceHandler::OnUploadProgress(int request_id,
                                                   uint64 position,
                                                   uint64 size) {
  return next_handler_->OnUploadProgress(request_id, position, size);
}

bool SafeBrowsingResourceHandler::OnRequestRedirected(
    int request_id,
    const GURL& new_url,
    ResourceResponse* response,
    bool* defer) {
  CHECK(state_ == STATE_NONE);
  CHECK(defer_state_ == DEFERRED_NONE);

  // Save the redirect urls for possible malware detail reporting later.
  redirect_urls_.push_back(new_url);

  // We need to check the new URL before following the redirect.
  if (CheckUrl(new_url)) {
    return next_handler_->OnRequestRedirected(
        request_id, new_url, response, defer);
  }

  // If the URL couldn't be verified synchronously, defer following the
  // redirect until the SafeBrowsing check is complete. Store the redirect
  // context so we can pass it on to other handlers once we have completed
  // our check.
  defer_state_ = DEFERRED_REDIRECT;
  deferred_request_id_ = request_id;
  deferred_url_ = new_url;
  deferred_redirect_response_ = response;
  *defer = true;

  return true;
}

bool SafeBrowsingResourceHandler::OnResponseStarted(
    int request_id, ResourceResponse* response) {
  CHECK(state_ == STATE_NONE);
  CHECK(defer_state_ == DEFERRED_NONE);
  return next_handler_->OnResponseStarted(request_id, response);
}

void SafeBrowsingResourceHandler::OnCheckUrlTimeout() {
  CHECK(state_ == STATE_CHECKING_URL);
  CHECK(defer_state_ != DEFERRED_NONE);
  safe_browsing_->CancelCheck(this);
  OnBrowseUrlCheckResult(deferred_url_, SafeBrowsingService::SAFE);
}

bool SafeBrowsingResourceHandler::OnWillStart(int request_id,
                                              const GURL& url,
                                              bool* defer) {
  // We need to check the new URL before starting the request.
  if (CheckUrl(url))
    return next_handler_->OnWillStart(request_id, url, defer);

  // If the URL couldn't be verified synchronously, defer starting the
  // request until the check has completed.
  defer_state_ = DEFERRED_START;
  deferred_request_id_ = request_id;
  deferred_url_ = url;
  *defer = true;

  return true;
}

bool SafeBrowsingResourceHandler::OnWillRead(int request_id,
                                             net::IOBuffer** buf, int* buf_size,
                                             int min_size) {
  CHECK(state_ == STATE_NONE);
  CHECK(defer_state_ == DEFERRED_NONE);
  return next_handler_->OnWillRead(request_id, buf, buf_size, min_size);
}

bool SafeBrowsingResourceHandler::OnReadCompleted(int request_id,
                                                  int* bytes_read) {
  CHECK(state_ == STATE_NONE);
  CHECK(defer_state_ == DEFERRED_NONE);
  return next_handler_->OnReadCompleted(request_id, bytes_read);
}

bool SafeBrowsingResourceHandler::OnResponseCompleted(
    int request_id, const net::URLRequestStatus& status,
    const std::string& security_info) {
  Shutdown();
  return next_handler_->OnResponseCompleted(request_id, status, security_info);
}

void SafeBrowsingResourceHandler::OnRequestClosed() {
  Shutdown();
  next_handler_->OnRequestClosed();
}

// SafeBrowsingService::Client implementation, called on the IO thread once
// the URL has been classified.
void SafeBrowsingResourceHandler::OnBrowseUrlCheckResult(
    const GURL& url, SafeBrowsingService::UrlCheckResult result) {
  CHECK(state_ == STATE_CHECKING_URL);
  CHECK(defer_state_ != DEFERRED_NONE);
  CHECK(url == deferred_url_) << "Was expecting: " << deferred_url_
                              << " but got: " << url;

  timer_.Stop();  // Cancel the timeout timer.
  safe_browsing_result_ = result;
  state_ = STATE_NONE;

  if (result == SafeBrowsingService::SAFE) {
    // Log how much time the safe browsing check cost us.
    base::TimeDelta pause_delta;
    pause_delta = base::TimeTicks::Now() - url_check_start_time_;
    safe_browsing_->LogPauseDelay(pause_delta);

    // Continue the request.
    ResumeRequest();
  } else {
    StartDisplayingBlockingPage(url, result);
  }

  Release();  // Balances the AddRef() in CheckingUrl().
}

void SafeBrowsingResourceHandler::StartDisplayingBlockingPage(
    const GURL& url,
    SafeBrowsingService::UrlCheckResult result) {
  CHECK(state_ == STATE_NONE);
  CHECK(defer_state_ != DEFERRED_NONE);
  CHECK(deferred_request_id_ != -1);

  state_ = STATE_DISPLAYING_BLOCKING_PAGE;
  AddRef();  // Balanced in OnBlockingPageComplete().

  // Grab the original url of this request as well.
  GURL original_url;
  net::URLRequest* request = rdh_->GetURLRequest(
      GlobalRequestID(render_process_host_id_, deferred_request_id_));
  if (request)
    original_url = request->original_url();
  else
    original_url = url;

  safe_browsing_->DisplayBlockingPage(
      url, original_url, redirect_urls_, resource_type_,
      result, this, render_process_host_id_, render_view_id_);
}

// SafeBrowsingService::Client implementation, called on the IO thread when
// the user has decided to proceed with the current request, or go back.
void SafeBrowsingResourceHandler::OnBlockingPageComplete(bool proceed) {
  CHECK(state_ == STATE_DISPLAYING_BLOCKING_PAGE);
  state_ = STATE_NONE;

  if (proceed) {
    safe_browsing_result_ = SafeBrowsingService::SAFE;
    ResumeRequest();
  } else {
    rdh_->CancelRequest(render_process_host_id_, deferred_request_id_, false);
  }

  Release();  // Balances the AddRef() in StartDisplayingBlockingPage().
}

void SafeBrowsingResourceHandler::Shutdown() {
  if (state_ == STATE_CHECKING_URL) {
    timer_.Stop();
    safe_browsing_->CancelCheck(this);
    state_ = STATE_NONE;
    // Balance the AddRef() from CheckUrl() which would ordinarily be
    // balanced by OnUrlCheckResult().
    Release();
  }
}

bool SafeBrowsingResourceHandler::CheckUrl(const GURL& url) {
  CHECK(state_ == STATE_NONE);
  bool succeeded_synchronously = safe_browsing_->CheckBrowseUrl(url, this);
  if (succeeded_synchronously) {
    safe_browsing_result_ = SafeBrowsingService::SAFE;
    safe_browsing_->LogPauseDelay(base::TimeDelta());  // No delay.
    return true;
  }

  AddRef();  // Balanced in OnUrlCheckResult().
  state_ = STATE_CHECKING_URL;

  // Record the start time of the check.
  url_check_start_time_ = base::TimeTicks::Now();

  // Start a timer to abort the check if it takes too long.
  timer_.Start(base::TimeDelta::FromMilliseconds(kCheckUrlTimeoutMs),
               this, &SafeBrowsingResourceHandler::OnCheckUrlTimeout);

  return false;
}

void SafeBrowsingResourceHandler::ResumeRequest() {
  CHECK(state_ == STATE_NONE);
  CHECK(defer_state_ != DEFERRED_NONE);

  // Resume whatever stage got paused by the safe browsing check.
  switch (defer_state_) {
    case DEFERRED_START:
      ResumeStart();
      break;
    case DEFERRED_REDIRECT:
      ResumeRedirect();
      break;
    case DEFERRED_NONE:
      NOTREACHED();
      break;
  }
}

void SafeBrowsingResourceHandler::ResumeStart() {
  CHECK(defer_state_ == DEFERRED_START);
  CHECK(deferred_request_id_ != -1);
  defer_state_ = DEFERRED_NONE;

  // Retrieve the details for the paused OnWillStart().
  int request_id = deferred_request_id_;
  GURL url = deferred_url_;

  ClearDeferredRequestInfo();

  // Give the other resource handlers a chance to defer starting.
  bool defer = false;
  // TODO(eroman): the return value is being lost here. Should
  // use it to cancel the request.
  next_handler_->OnWillStart(request_id, url, &defer);
  if (!defer)
    rdh_->StartDeferredRequest(render_process_host_id_, request_id);
}

void SafeBrowsingResourceHandler::ResumeRedirect() {
  CHECK(defer_state_ == DEFERRED_REDIRECT);
  defer_state_ = DEFERRED_NONE;

  // Retrieve the details for the paused OnReceivedRedirect().
  int request_id = deferred_request_id_;
  GURL redirect_url = deferred_url_;
  scoped_refptr<ResourceResponse> redirect_response =
      deferred_redirect_response_;

  ClearDeferredRequestInfo();

  // Give the other resource handlers a chance to handle the redirect.
  bool defer = false;
  // TODO(eroman): the return value is being lost here. Should
  // use it to cancel the request.
  next_handler_->OnRequestRedirected(request_id, redirect_url,
                                     redirect_response, &defer);
  if (!defer) {
    rdh_->FollowDeferredRedirect(render_process_host_id_, request_id,
                                 false, GURL());
  }
}

void SafeBrowsingResourceHandler::ClearDeferredRequestInfo() {
  deferred_request_id_ = -1;
  deferred_url_ = GURL();
  deferred_redirect_response_ = NULL;
}

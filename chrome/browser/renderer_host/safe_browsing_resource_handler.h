// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_SAFE_BROWSING_RESOURCE_HANDLER_H_
#define CHROME_BROWSER_RENDERER_HOST_SAFE_BROWSING_RESOURCE_HANDLER_H_
#pragma once

#include <string>
#include <vector>

#include "base/ref_counted.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "content/browser/renderer_host/resource_handler.h"

class ResourceDispatcherHost;

// SafeBrowsingResourceHandler checks that URLs are "safe" before navigating
// to them. To be considered "safe", a URL must not appear in the
// malware/phishing blacklists (see SafeBrowsingService for details).
//
// This check is done before requesting the original URL, and additionally
// before following any subsequent redirect.
//
// In the common case, the check completes synchronously (no match in the bloom
// filter), so the request's flow is un-interrupted.
//
// However if the URL fails this quick check, it has the possibility of being
// on the blacklist. Now the request is suspended (prevented from starting),
// and a more expensive safe browsing check is begun (fetches the full hashes).
//
// Note that the safe browsing check takes at most kCheckUrlTimeoutMs
// milliseconds. If it takes longer than this, then the system defaults to
// treating the URL as safe.
//
// Once the safe browsing check has completed, if the URL was decided to be
// dangerous, a warning page is thrown up and the request remains suspended.
// If on the other hand the URL was decided to be safe, the request is
// resumed.
class SafeBrowsingResourceHandler : public ResourceHandler,
                                    public SafeBrowsingService::Client {
 public:
  SafeBrowsingResourceHandler(ResourceHandler* handler,
                              int render_process_host_id,
                              int render_view_id,
                              ResourceType::Type resource_type,
                              SafeBrowsingService* safe_browsing,
                              ResourceDispatcherHost* resource_dispatcher_host);

  // ResourceHandler implementation:
  virtual bool OnUploadProgress(int request_id, uint64 position, uint64 size);
  virtual bool OnRequestRedirected(int request_id, const GURL& new_url,
                                   ResourceResponse* response, bool* defer);
  virtual bool OnResponseStarted(int request_id, ResourceResponse* response);
  virtual bool OnWillStart(int request_id, const GURL& url, bool* defer);
  virtual bool OnWillRead(int request_id, net::IOBuffer** buf, int* buf_size,
                          int min_size);
  virtual bool OnReadCompleted(int request_id, int* bytes_read);
  virtual bool OnResponseCompleted(int request_id,
                                   const net::URLRequestStatus& status,
                                   const std::string& security_info);
  virtual void OnRequestClosed();

  // SafeBrowsingService::Client implementation, called on the IO thread once
  // the URL has been classified.
  virtual void OnBrowseUrlCheckResult(
      const GURL& url, SafeBrowsingService::UrlCheckResult result);

  // SafeBrowsingService::Client implementation, called on the IO thread when
  // the user has decided to proceed with the current request, or go back.
  virtual void OnBlockingPageComplete(bool proceed);

 private:
  // Describes what phase of the check a handler is in.
  enum State {
    STATE_NONE,
    STATE_CHECKING_URL,
    STATE_DISPLAYING_BLOCKING_PAGE,
  };

  // Describes what stage of the request got paused by the check.
  enum DeferState {
    DEFERRED_NONE,
    DEFERRED_START,
    DEFERRED_REDIRECT,
  };

  ~SafeBrowsingResourceHandler();

  // Cancels any in progress safe browsing actions.
  void Shutdown();

  // Starts running |url| through the safe browsing check. Returns true if the
  // URL is safe to visit. Otherwise returns false and will call
  // OnUrlCheckResult() when the check has completed.
  bool CheckUrl(const GURL& url);

  // Callback for when the safe browsing check (which was initiated by
  // StartCheckingUrl()) has taken longer than kCheckUrlTimeoutMs.
  void OnCheckUrlTimeout();

  // Starts displaying the safe browsing interstitial page.
  void StartDisplayingBlockingPage(const GURL& url,
                                   SafeBrowsingService::UrlCheckResult result);

  // Resumes the request, by continuing the deferred action (either starting the
  // request, or following a redirect).
  void ResumeRequest();

  // Resumes the deferred "start".
  void ResumeStart();

  // Resumes the deferred redirect.
  void ResumeRedirect();

  // Erases the state associated with a deferred "start" or redirect
  // (i.e. the deferred URL and request ID).
  void ClearDeferredRequestInfo();

  State state_;
  DeferState defer_state_;

  // The result of the most recent safe browsing check. Only valid to read this
  // when state_ != STATE_CHECKING_URL.
  SafeBrowsingService::UrlCheckResult safe_browsing_result_;

  // The time when the outstanding safe browsing check was started.
  base::TimeTicks url_check_start_time_;

  // Timer to abort the safe browsing check if it takes too long.
  base::OneShotTimer<SafeBrowsingResourceHandler> timer_;

  // The redirect chain for this resource
  std::vector<GURL> redirect_urls_;

  // Details on the deferred request (either a start or redirect). It is only
  // valid to access these members when defer_state_ != DEFERRED_NONE.
  GURL deferred_url_;
  int deferred_request_id_;
  scoped_refptr<ResourceResponse> deferred_redirect_response_;

  scoped_refptr<ResourceHandler> next_handler_;
  int render_process_host_id_;
  int render_view_id_;
  scoped_refptr<SafeBrowsingService> safe_browsing_;
  ResourceDispatcherHost* rdh_;
  ResourceType::Type resource_type_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingResourceHandler);
};


#endif  // CHROME_BROWSER_RENDERER_HOST_SAFE_BROWSING_RESOURCE_HANDLER_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_SAFE_BROWSING_RESOURCE_THROTTLE_H_
#define CHROME_BROWSER_RENDERER_HOST_SAFE_BROWSING_RESOURCE_THROTTLE_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/safe_browsing/database_manager.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "content/public/browser/resource_throttle.h"

class ResourceDispatcherHost;

namespace net {
class URLRequest;
}

// SafeBrowsingResourceThrottle checks that URLs are "safe" before navigating
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
class SafeBrowsingResourceThrottle
    : public content::ResourceThrottle,
      public SafeBrowsingDatabaseManager::Client,
      public base::SupportsWeakPtr<SafeBrowsingResourceThrottle> {
 public:
  SafeBrowsingResourceThrottle(const net::URLRequest* request,
                               bool is_subresource,
                               SafeBrowsingService* safe_browsing);

  // content::ResourceThrottle implementation (called on IO thread):
  virtual void WillStartRequest(bool* defer) OVERRIDE;
  virtual void WillRedirectRequest(const GURL& new_url, bool* defer) OVERRIDE;
  virtual const char* GetNameForLogging() const OVERRIDE;

  // SafeBrowsingDabaseManager::Client implementation (called on IO thread):
  virtual void OnCheckBrowseUrlResult(
      const GURL& url, SBThreatType result) OVERRIDE;

 private:
  // Describes what phase of the check a throttle is in.
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

  virtual ~SafeBrowsingResourceThrottle();

  // SafeBrowsingService::UrlCheckCallback implementation.
  void OnBlockingPageComplete(bool proceed);

  // Starts running |url| through the safe browsing check. Returns true if the
  // URL is safe to visit. Otherwise returns false and will call
  // OnBrowseUrlResult() when the check has completed.
  bool CheckUrl(const GURL& url);

  // Callback for when the safe browsing check (which was initiated by
  // StartCheckingUrl()) has taken longer than kCheckUrlTimeoutMs.
  void OnCheckUrlTimeout();

  // Starts displaying the safe browsing interstitial page if it's not
  // prerendering. Called on the UI thread.
  static void StartDisplayingBlockingPage(
      const base::WeakPtr<SafeBrowsingResourceThrottle>& throttle,
      scoped_refptr<SafeBrowsingUIManager> ui_manager,
      const SafeBrowsingUIManager::UnsafeResource& resource);

  // Called on the IO thread if the request turned out to be for a prerendered
  // page.
  void Cancel();

  // Resumes the request, by continuing the deferred action (either starting the
  // request, or following a redirect).
  void ResumeRequest();

  State state_;
  DeferState defer_state_;

  // The result of the most recent safe browsing check. Only valid to read this
  // when state_ != STATE_CHECKING_URL.
  SBThreatType threat_type_;

  // The time when the outstanding safe browsing check was started.
  base::TimeTicks url_check_start_time_;

  // Timer to abort the safe browsing check if it takes too long.
  base::OneShotTimer<SafeBrowsingResourceThrottle> timer_;

  // The redirect chain for this resource
  std::vector<GURL> redirect_urls_;

  GURL url_being_checked_;

  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;
  scoped_refptr<SafeBrowsingUIManager> ui_manager_;
  const net::URLRequest* request_;
  bool is_subresource_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingResourceThrottle);
};


#endif  // CHROME_BROWSER_RENDERER_HOST_SAFE_BROWSING_RESOURCE_THROTTLE_H_

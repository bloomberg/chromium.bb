// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_BROWSER_BROWSER_URL_LOADER_THROTTLE_H_
#define COMPONENTS_SAFE_BROWSING_BROWSER_BROWSER_URL_LOADER_THROTTLE_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "content/public/common/url_loader_throttle.h"

namespace content {
class WebContents;
}

namespace safe_browsing {

class NetEventLogger;
class SafeBrowsingUrlCheckerImpl;
class UrlCheckerDelegate;

// BrowserURLLoaderThrottle is used in the browser process to query
// SafeBrowsing to determine whether a URL and also its redirect URLs are safe
// to load. It defers response processing until all URL checks are completed;
// cancels the load if any URLs turn out to be bad.
class BrowserURLLoaderThrottle : public content::URLLoaderThrottle {
 public:
  static std::unique_ptr<BrowserURLLoaderThrottle> MaybeCreate(
      scoped_refptr<UrlCheckerDelegate> url_checker_delegate,
      const base::Callback<content::WebContents*()>& web_contents_getter);

  ~BrowserURLLoaderThrottle() override;

  // content::URLLoaderThrottle implementation.
  void WillStartRequest(const content::ResourceRequest& request,
                        bool* defer) override;
  void WillRedirectRequest(const net::RedirectInfo& redirect_info,
                           bool* defer) override;
  void WillProcessResponse(bool* defer) override;

  void set_net_event_logger(NetEventLogger* net_event_logger);

 private:
  using NativeUrlCheckNotifier =
      base::OnceCallback<void(bool /* proceed */,
                              bool /* showed_interstitial */)>;

  // |web_contents_getter| is used for displaying SafeBrowsing UI when
  // necessary.
  BrowserURLLoaderThrottle(
      scoped_refptr<UrlCheckerDelegate> url_checker_delegate,
      const base::Callback<content::WebContents*()>& web_contents_getter);

  // |slow_check| indicates whether it reports the result of a slow check.
  // (Please see comments of OnCheckUrlResult() for what slow check means).
  void OnCompleteCheck(bool slow_check, bool proceed, bool showed_interstitial);

  // If |slow_check_notifier| is non-null, it indicates that a "slow check" is
  // ongoing, i.e., the URL may be unsafe and a more time-consuming process is
  // required to get the final result. In that case, the rest of the callback
  // arguments should be ignored. This method sets the |slow_check_notifier|
  // output parameter to a callback to receive the final result.
  void OnCheckUrlResult(NativeUrlCheckNotifier* slow_check_notifier,
                        bool proceed,
                        bool showed_interstitial);

  // The following member stays valid until |url_checker_| is created.
  scoped_refptr<UrlCheckerDelegate> url_checker_delegate_;

  base::Callback<content::WebContents*()> web_contents_getter_;

  std::unique_ptr<SafeBrowsingUrlCheckerImpl> url_checker_;

  size_t pending_checks_ = 0;
  // How many slow checks that haven't received results.
  size_t pending_slow_checks_ = 0;
  bool blocked_ = false;

  // The time when we started deferring the request.
  base::TimeTicks defer_start_time_;
  bool deferred_ = false;

  NetEventLogger* net_event_logger_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(BrowserURLLoaderThrottle);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_BROWSER_BROWSER_URL_LOADER_THROTTLE_H_

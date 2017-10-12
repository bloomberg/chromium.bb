// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_BROWSER_BASE_PARALLEL_RESOURCE_THROTTLE_H_
#define COMPONENTS_SAFE_BROWSING_BROWSER_BASE_PARALLEL_RESOURCE_THROTTLE_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/safe_browsing/net_event_logger.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/common/resource_type.h"
#include "content/public/common/url_loader_throttle.h"

namespace net {
class URLRequest;
}

namespace safe_browsing {

class BrowserURLLoaderThrottle;
class UrlCheckerDelegate;

// A thin wrapper around BrowserURLLoaderThrottle to adapt to the
// content::ResourceThrottle interface.
// Used when --enable-features=S13nSafeBrowsingParallelUrlCheck is in effect.
class BaseParallelResourceThrottle : public content::ResourceThrottle {
 protected:
  BaseParallelResourceThrottle(
      const net::URLRequest* request,
      content::ResourceType resource_type,
      scoped_refptr<UrlCheckerDelegate> url_checker_delegate);

  ~BaseParallelResourceThrottle() override;

  // content::ResourceThrottle implementation:
  void WillStartRequest(bool* defer) override;
  void WillRedirectRequest(const net::RedirectInfo& redirect_info,
                           bool* defer) override;
  void WillProcessResponse(bool* defer) override;
  const char* GetNameForLogging() const override;
  bool MustProcessResponseBeforeReadingBody() override;

  virtual void CancelResourceLoad();

 private:
  class URLLoaderThrottleHolder;

  const net::URLRequest* const request_;
  const content::ResourceType resource_type_;
  NetEventLogger net_event_logger_;

  std::unique_ptr<URLLoaderThrottleHolder> url_loader_throttle_holder_;

  DISALLOW_COPY_AND_ASSIGN(BaseParallelResourceThrottle);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_BROWSER_BASE_PARALLEL_RESOURCE_THROTTLE_H_

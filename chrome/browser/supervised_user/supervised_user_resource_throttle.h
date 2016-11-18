// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_RESOURCE_THROTTLE_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_RESOURCE_THROTTLE_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#include "chrome/browser/supervised_user/supervised_users.h"
#include "components/supervised_user_error_page/supervised_user_error_page.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/common/resource_type.h"

namespace net {
class URLRequest;
}

class SupervisedUserResourceThrottle : public content::ResourceThrottle {
 public:
  // Returns a new throttle for the given parameters, or nullptr if no
  // throttling is required.
  static std::unique_ptr<SupervisedUserResourceThrottle> MaybeCreate(
      const net::URLRequest* request,
      content::ResourceType resource_type,
      const SupervisedUserURLFilter* url_filter);

  ~SupervisedUserResourceThrottle() override;

  // content::ResourceThrottle implementation:
  void WillStartRequest(bool* defer) override;

  void WillRedirectRequest(const net::RedirectInfo& redirect_info,
                           bool* defer) override;

  const char* GetNameForLogging() const override;

 private:
  SupervisedUserResourceThrottle(const net::URLRequest* request,
                                 const SupervisedUserURLFilter* url_filter);

  void CheckURL(const GURL& url, bool* defer);

  void ShowInterstitial(
      const GURL& url,
      supervised_user_error_page::FilteringBehaviorReason reason);

  void OnCheckDone(const GURL& url,
                   SupervisedUserURLFilter::FilteringBehavior behavior,
                   supervised_user_error_page::FilteringBehaviorReason reason,
                   bool uncertain);

  void OnInterstitialResult(bool continue_request);

  const net::URLRequest* request_;
  const SupervisedUserURLFilter* url_filter_;
  bool deferred_;
  SupervisedUserURLFilter::FilteringBehavior behavior_;
  base::WeakPtrFactory<SupervisedUserResourceThrottle> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserResourceThrottle);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_RESOURCE_THROTTLE_H_

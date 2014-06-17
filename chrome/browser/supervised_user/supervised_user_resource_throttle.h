// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_RESOURCE_THROTTLE_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_RESOURCE_THROTTLE_H_

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/supervised_user/supervised_users.h"
#include "content/public/browser/resource_throttle.h"

class SupervisedUserURLFilter;

namespace net {
class URLRequest;
}

class SupervisedUserResourceThrottle : public content::ResourceThrottle {
 public:
  SupervisedUserResourceThrottle(const net::URLRequest* request,
                                 bool is_main_frame,
                                 const SupervisedUserURLFilter* url_filter);
  virtual ~SupervisedUserResourceThrottle();

  // content::ResourceThrottle implementation:
  virtual void WillStartRequest(bool* defer) OVERRIDE;

  virtual void WillRedirectRequest(const GURL& new_url, bool* defer) OVERRIDE;

  virtual const char* GetNameForLogging() const OVERRIDE;

 private:
  void ShowInterstitialIfNeeded(bool is_redirect,
                                const GURL& url,
                                bool* defer);
  void OnInterstitialResult(bool continue_request);

  const net::URLRequest* request_;
  bool is_main_frame_;
  const SupervisedUserURLFilter* url_filter_;
  base::WeakPtrFactory<SupervisedUserResourceThrottle> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserResourceThrottle);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_RESOURCE_THROTTLE_H_

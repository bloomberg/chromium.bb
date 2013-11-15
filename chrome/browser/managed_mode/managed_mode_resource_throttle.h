// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANAGED_MODE_MANAGED_MODE_RESOURCE_THROTTLE_H_
#define CHROME_BROWSER_MANAGED_MODE_MANAGED_MODE_RESOURCE_THROTTLE_H_

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/managed_mode/managed_users.h"
#include "content/public/browser/resource_throttle.h"

class ManagedModeURLFilter;

namespace net {
class URLRequest;
}

class ManagedModeResourceThrottle : public content::ResourceThrottle {
 public:
  ManagedModeResourceThrottle(const net::URLRequest* request,
                              bool is_main_frame,
                              const ManagedModeURLFilter* url_filter);
  virtual ~ManagedModeResourceThrottle();

  // content::ResourceThrottle implementation:
  virtual void WillStartRequest(bool* defer) OVERRIDE;

  virtual void WillRedirectRequest(const GURL& new_url, bool* defer) OVERRIDE;

 private:
  void ShowInterstitialIfNeeded(bool is_redirect,
                                const GURL& url,
                                bool* defer);
  void OnInterstitialResult(bool continue_request);

  const net::URLRequest* request_;
  bool is_main_frame_;
  const ManagedModeURLFilter* url_filter_;
  base::WeakPtrFactory<ManagedModeResourceThrottle> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ManagedModeResourceThrottle);
};

#endif  // CHROME_BROWSER_MANAGED_MODE_MANAGED_MODE_RESOURCE_THROTTLE_H_

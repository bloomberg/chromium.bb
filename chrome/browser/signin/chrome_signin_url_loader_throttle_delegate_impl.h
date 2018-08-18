// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_URL_LOADER_THROTTLE_DELEGATE_IMPL_H_
#define CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_URL_LOADER_THROTTLE_DELEGATE_IMPL_H_

#include "chrome/browser/signin/chrome_signin_url_loader_throttle.h"

class ProfileIOData;

namespace signin {

class URLLoaderThrottleDelegateImpl : public URLLoaderThrottle::Delegate {
 public:
  explicit URLLoaderThrottleDelegateImpl(
      content::ResourceContext* resource_context);
  ~URLLoaderThrottleDelegateImpl() override;

  // URLLoaderThrottle::Delegate
  bool ShouldIntercept(content::NavigationUIData* navigation_ui_data) override;
  void ProcessRequest(ChromeRequestAdapter* request_adapter,
                      const GURL& redirect_url) override;
  void ProcessResponse(ResponseAdapter* response_adapter,
                       const GURL& redirect_url) override;

 private:
  ProfileIOData* const io_data_;

  DISALLOW_COPY_AND_ASSIGN(URLLoaderThrottleDelegateImpl);
};

}  // namespace signin

#endif  // CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_URL_LOADER_THROTTLE_DELEGATE_IMPL_H_

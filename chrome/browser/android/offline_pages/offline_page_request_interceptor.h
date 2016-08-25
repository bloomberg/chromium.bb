// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_REQUEST_INTERCEPTOR_H_
#define CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_REQUEST_INTERCEPTOR_H_

#include "base/macros.h"
#include "net/url_request/url_request_interceptor.h"

namespace net {
class NetworkDelegate;
class URLRequest;
class URLRequestJob;
}

namespace offline_pages {

// An interceptor to hijack requests and potentially service them based on
// their offline information. Created one per profile.
class OfflinePageRequestInterceptor : public net::URLRequestInterceptor {
 public:
  // |profile_id|, which identifies the profile, is passed as a void* to ensure
  // it's not accidently used on the IO thread.
  explicit OfflinePageRequestInterceptor(void* profile_id);
  ~OfflinePageRequestInterceptor() override;

 private:
  // Overrides from net::URLRequestInterceptor:
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override;

  // The profile for processing offline pages.
  void* const profile_id_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageRequestInterceptor);
};

}  // namespace offline_pages

#endif // CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_REQUEST_INTERCEPTOR_H_

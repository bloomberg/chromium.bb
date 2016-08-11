// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_REQUEST_HANDLER_H_
#define CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_REQUEST_HANDLER_H_

#include "base/macros.h"
#include "base/supports_user_data.h"
#include "content/public/common/resource_type.h"

namespace net {
class NetworkDelegate;
class URLRequest;
class URLRequestInterceptor;
class URLRequestJob;
}

namespace offline_pages {

// Class for servicing requests based on their offline information. Created one
// per URLRequest and attached to each request.
//
// For each supported profile, OfflinePageRequestHandler::CreateInterceptor
// should be called once to install the custom interceptor.
//
// For each request:
// 1) When a request is starting, OfflinePageRequestHandler::InitializeHandler
//    will be called to create a new handler and attach to the request.
// 2) When a request is being processed, MaybeInterceptRequest of custom
//    interceptor will be inquired.
//    2.1) If the attached OfflinePageRequestHandler for the request is found,
//         delegate to OfflinePageRequestHandler::MaybeCreateJob to do the work.
//         2.1.1) Do immediate checks for those scenarios that the interception
//                is not needed. Bail out if so.
//         2.1.2) Start an async task to try to find the offline page.
//         2.1.3) Return a custom URLRequestJob that is put on hold to wait
//                for the result of finding offline page.
//    2.2) Otherwise, bail out without interception.
class OfflinePageRequestHandler : public base::SupportsUserData::Data {
 public:
  // Attaches a newly created handler if the given |request| needs to
  // be handled by offline pages.
  static void InitializeHandler(net::URLRequest* request,
                                content::ResourceType resource_type);

  // Returns the handler attached to |request|. This may return null if no
  // handler is attached.
  static OfflinePageRequestHandler* GetHandler(net::URLRequest* request);

  // Creates a protocol interceptor for offline pages. Created one per
  // supported, i.e. non-incognito, profile.
  // |profile_id|, which identifies the profile, is passed as a void* to ensure
  // it's not accidently used on the IO thread.
  static std::unique_ptr<net::URLRequestInterceptor> CreateInterceptor(
      void* profile_id);

  ~OfflinePageRequestHandler() override;

  net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      void* profile_id);

 private:
  OfflinePageRequestHandler();

  DISALLOW_COPY_AND_ASSIGN(OfflinePageRequestHandler);
};

}  // namespace offline_pages

#endif // CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_REQUEST_HANDLER_H_

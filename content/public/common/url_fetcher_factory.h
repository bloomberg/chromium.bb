// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_URL_FETCHER_FACTORY_H_
#define CONTENT_PUBLIC_COMMON_URL_FETCHER_FACTORY_H_
#pragma once

#include "content/public/common/url_fetcher.h"

namespace net {
class URLFetcherDelegate;
}  // namespace net

namespace content {

// URLFetcher::Create uses the currently registered Factory to create the
// URLFetcher. Factory is intended for testing.
class URLFetcherFactory {
 public:
  virtual URLFetcher* CreateURLFetcher(
      int id,
      const GURL& url,
      URLFetcher::RequestType request_type,
      net::URLFetcherDelegate* d) = 0;

 protected:
  virtual ~URLFetcherFactory() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_URL_FETCHER_FACTORY_H_

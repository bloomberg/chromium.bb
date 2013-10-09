// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_DISTILLER_URL_FETCHER_H_
#define COMPONENTS_DOM_DISTILLER_CORE_DISTILLER_URL_FETCHER_H_

#include "base/callback.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"

namespace dom_distiller {

// This class fetches a URL, and notifies the caller when the operation
// completes or fails. If the request fails, an empty string will be returned.
class DistillerURLFetcher : public net::URLFetcherDelegate {
 public:
  DistillerURLFetcher();
  virtual ~DistillerURLFetcher();

  // Indicates when a fetch is done.
  typedef base::Callback<void(const std::string& data)> URLFetcherCallback;

  // Fetches a |url|. Notifies when the fetch is done via |callback|.
  void FetchURL(net::URLRequestContextGetter* context_getter,
                const std::string& url,
                const URLFetcherCallback& callback);

 protected:
  virtual net::URLFetcher* CreateURLFetcher(
      net::URLRequestContextGetter* context_getter,
      const std::string& url);

 private:
  // net::URLFetcherDelegate:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  scoped_ptr<net::URLFetcher> url_fetcher_;
  URLFetcherCallback callback_;
  DISALLOW_COPY_AND_ASSIGN(DistillerURLFetcher);
};

} // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_DISTILLER_URL_FETCHER_H_

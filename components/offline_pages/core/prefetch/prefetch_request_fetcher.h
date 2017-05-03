// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_REQUEST_FETCHER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_REQUEST_FETCHER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "net/url_request/url_fetcher_delegate.h"

class GURL;
namespace net {
class URLRequestContextGetter;
}

namespace offline_prefetch {

class PrefetchRequestFetcher : public net::URLFetcherDelegate {
 public:
  enum class Status {
    // Request completed successfully.
    SUCCESS,
    // Request failed due to to local network problem, unrelated to server load
    // levels. The caller will simply reschedule the retry in the next available
    // WiFi window after 15 minutes have passed.
    SHOULD_RETRY_WITHOUT_BACKOFF,
    // Request failed probably related to transient server problems. The caller
    // will reschedule the retry with backoff included.
    SHOULD_RETRY_WITH_BACKOFF,
    // Request failed with error indicating that the server no longer knows how
    // to service a request. The caller will prevent network requests for the
    // period of 1 day.
    SHOULD_SUSPEND
  };

  using FinishedCallback =
      base::Callback<void(Status status, const std::string& data)>;

  PrefetchRequestFetcher(
      const GURL& url,
      const std::string& message,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter,
      const FinishedCallback& callback);
  ~PrefetchRequestFetcher() override;

  // URLFetcherDelegate implementation.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

 private:
  Status ParseResponse(const net::URLFetcher* source, std::string* data);

  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  std::unique_ptr<net::URLFetcher> url_fetcher_;
  FinishedCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(PrefetchRequestFetcher);
};

}  // namespace offline_prefetch

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_REQUEST_FETCHER_H_

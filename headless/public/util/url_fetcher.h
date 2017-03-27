// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_UTIL_URL_FETCHER_H_
#define HEADLESS_PUBLIC_UTIL_URL_FETCHER_H_

#include <stddef.h>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "headless/public/headless_export.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

namespace net {
class HttpRequestHeaders;
class HttpResponseHeaders;
}  // namespace net

namespace headless {

// An interface for fetching URLs. Note these are only intended to be used once.
class HEADLESS_EXPORT URLFetcher {
 public:
  URLFetcher() {}
  virtual ~URLFetcher() {}

  // Interface for reporting the fetch result.
  class HEADLESS_EXPORT ResultListener {
   public:
    ResultListener() {}

    // Informs the listener that the fetch failed.
    virtual void OnFetchStartError(net::Error error) = 0;

    // Informs the listener that the fetch succeeded and returns the HTTP
    // headers and the body.  NOTE |body| must be owned by the caller and remain
    // valid until the fetcher is destroyed,
    virtual void OnFetchComplete(
        const GURL& final_url,
        int http_response_code,
        scoped_refptr<net::HttpResponseHeaders> response_headers,
        const char* body,
        size_t body_size) = 0;

    // Helper function which extracts the headers from |response_data| and calls
    // OnFetchComplete.
    void OnFetchCompleteExtractHeaders(const GURL& final_url,
                                       int http_response_code,
                                       const char* response_data,
                                       size_t response_data_size);

   protected:
    virtual ~ResultListener() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(ResultListener);
  };

  // Instructs the sub-class to fetch the resource.
  virtual void StartFetch(const GURL& rewritten_url,
                          const std::string& method,
                          const net::HttpRequestHeaders& request_headers,
                          const std::string& devtools_request_id,
                          ResultListener* result_listener);
  //  TODO(alexclarke): Make the above pure virtual and remove this.
  virtual void StartFetch(const GURL& rewritten_url,
                          const std::string& method,
                          const net::HttpRequestHeaders& request_headers,
                          ResultListener* result_listener);

 private:
  DISALLOW_COPY_AND_ASSIGN(URLFetcher);
};

}  // namespace headless

#endif  // HEADLESS_PUBLIC_UTIL_URL_FETCHER_H_

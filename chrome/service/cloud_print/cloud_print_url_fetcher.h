// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_URL_FETCHER_H_
#define CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_URL_FETCHER_H_
#pragma once

#include <string>

#include "base/scoped_ptr.h"
#include "chrome/common/net/url_fetcher.h"

class DictionaryValue;
class GURL;

namespace net {
class URLRequestStatus;
}  // namespace net

// A wrapper around URLFetcher for CloudPrint. URLFetcher applies retry logic
// only on HTTP response codes >= 500. In the cloud print case, we want to
// retry on all network errors. In addition, we want to treat non-JSON responses
// (for all CloudPrint APIs that expect JSON responses) as errors and they
// must also be retried.
class CloudPrintURLFetcher
    : public base::RefCountedThreadSafe<CloudPrintURLFetcher>,
      public URLFetcher::Delegate {
 public:
  enum ResponseAction {
    CONTINUE_PROCESSING,
    STOP_PROCESSING,
    RETRY_REQUEST,
  };
  class Delegate {
   public:
    virtual ~Delegate() { }
    // Override this to handle the raw response as it is available. No response
    // error checking is done before this method is called. If the delegate
    // returns CONTINUE_PROCESSING, we will then check for network
    // errors. Most implementations will not override this.
    virtual ResponseAction HandleRawResponse(
        const URLFetcher* source,
        const GURL& url,
        const net::URLRequestStatus& status,
        int response_code,
        const ResponseCookies& cookies,
        const std::string& data) {
      return CONTINUE_PROCESSING;
    }
    // This will be invoked only if HandleRawResponse returns
    // CONTINUE_PROCESSING AND if there are no network errors and the HTTP
    // response code is 200. The delegate implementation returns
    // CONTINUE_PROCESSING if it does not want to handle the raw data itself.
    // Handling the raw data is needed when the expected response is NOT JSON
    // (like in the case of a print ticket response or a print job download
    // response).
    virtual ResponseAction HandleRawData(const URLFetcher* source,
                                         const GURL& url,
                                         const std::string& data) {
      return CONTINUE_PROCESSING;
    }
    // This will be invoked only if HandleRawResponse and HandleRawData return
    // CONTINUE_PROCESSING AND if the response contains a valid JSON dictionary.
    // |succeeded| is the value of the "success" field in the response JSON.
    virtual ResponseAction HandleJSONData(const URLFetcher* source,
                                          const GURL& url,
                                          DictionaryValue* json_data,
                                          bool succeeded) {
      return CONTINUE_PROCESSING;
    }
    // Invoked when the retry limit for this request has been reached (if there
    // was a retry limit - a limit of -1 implies no limit).
    virtual void OnRequestGiveUp() { }
    // Invoked when the request returns a 403 error (applicable only when
    // HandleRawResponse returns CONTINUE_PROCESSING)
    virtual void OnRequestAuthError() = 0;
  };
  CloudPrintURLFetcher();

  void StartGetRequest(const GURL& url,
                       Delegate* delegate,
                       const std::string& auth_token,
                       int max_retries,
                       const std::string& additional_headers);
  void StartPostRequest(const GURL& url,
                        Delegate* delegate,
                        const std::string& auth_token,
                        int max_retries,
                        const std::string& post_data_mime_type,
                        const std::string& post_data,
                        const std::string& additional_headers);

  // URLFetcher::Delegate implementation.
  virtual void OnURLFetchComplete(const URLFetcher* source, const GURL& url,
                                  const net::URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);
 protected:
  friend class base::RefCountedThreadSafe<CloudPrintURLFetcher>;
  virtual ~CloudPrintURLFetcher();

  // Virtual for testing.
  virtual URLRequestContextGetter* GetRequestContextGetter();

 private:
  void StartRequestHelper(const GURL& url,
                          URLFetcher::RequestType request_type,
                          Delegate* delegate,
                          const std::string& auth_token,
                          int max_retries,
                          const std::string& post_data_mime_type,
                          const std::string& post_data,
                          const std::string& additional_headers);

  scoped_ptr<URLFetcher> request_;
  Delegate* delegate_;
  int num_retries_;
};

typedef CloudPrintURLFetcher::Delegate CloudPrintURLFetcherDelegate;

#endif  // CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_URL_FETCHER_H_

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_URL_FETCHER_H_
#define CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_URL_FETCHER_H_
#pragma once

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/common/url_fetcher.h"
#include "content/public/common/url_fetcher_delegate.h"

class GURL;

namespace base {
class DictionaryValue;
}

namespace net {
class URLRequestContextGetter;
class URLRequestStatus;
}  // namespace net

// A wrapper around URLFetcher for CloudPrint. URLFetcher applies retry logic
// only on HTTP response codes >= 500. In the cloud print case, we want to
// retry on all network errors. In addition, we want to treat non-JSON responses
// (for all CloudPrint APIs that expect JSON responses) as errors and they
// must also be retried.
class CloudPrintURLFetcher
    : public base::RefCountedThreadSafe<CloudPrintURLFetcher>,
      public content::URLFetcherDelegate {
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
        const content::URLFetcher* source,
        const GURL& url,
        const net::URLRequestStatus& status,
        int response_code,
        const net::ResponseCookies& cookies,
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
    virtual ResponseAction HandleRawData(const content::URLFetcher* source,
                                         const GURL& url,
                                         const std::string& data) {
      return CONTINUE_PROCESSING;
    }
    // This will be invoked only if HandleRawResponse and HandleRawData return
    // CONTINUE_PROCESSING AND if the response contains a valid JSON dictionary.
    // |succeeded| is the value of the "success" field in the response JSON.
    virtual ResponseAction HandleJSONData(const content::URLFetcher* source,
                                          const GURL& url,
                                          base::DictionaryValue* json_data,
                                          bool succeeded) {
      return CONTINUE_PROCESSING;
    }
    // Invoked when the retry limit for this request has been reached (if there
    // was a retry limit - a limit of -1 implies no limit).
    virtual void OnRequestGiveUp() { }
    // Invoked when the request returns a 403 error (applicable only when
    // HandleRawResponse returns CONTINUE_PROCESSING).
    // Returning RETRY_REQUEST will retry current request. (auth information
    // may have been updated and new info is available through the
    // Authenticator interface).
    // Returning CONTINUE_PROCESSING will treat auth error as a network error.
    virtual ResponseAction OnRequestAuthError() = 0;

    // Authentication information may change between retries.
    // CloudPrintURLFetcher will request auth info before sending any request.
    virtual std::string GetAuthHeader() = 0;
  };
  CloudPrintURLFetcher();

  bool IsSameRequest(const content::URLFetcher* source);

  void StartGetRequest(const GURL& url,
                       Delegate* delegate,
                       int max_retries,
                       const std::string& additional_headers);
  void StartPostRequest(const GURL& url,
                        Delegate* delegate,
                        int max_retries,
                        const std::string& post_data_mime_type,
                        const std::string& post_data,
                        const std::string& additional_headers);

  // content::URLFetcherDelegate implementation.
  virtual void OnURLFetchComplete(const content::URLFetcher* source) OVERRIDE;

 protected:
  friend class base::RefCountedThreadSafe<CloudPrintURLFetcher>;
  virtual ~CloudPrintURLFetcher();

  // Virtual for testing.
  virtual net::URLRequestContextGetter* GetRequestContextGetter();

 private:
  void StartRequestHelper(const GURL& url,
                          content::URLFetcher::RequestType request_type,
                          Delegate* delegate,
                          int max_retries,
                          const std::string& post_data_mime_type,
                          const std::string& post_data,
                          const std::string& additional_headers);
  void SetupRequestHeaders();

  scoped_ptr<content::URLFetcher> request_;
  Delegate* delegate_;
  int num_retries_;
  content::URLFetcher::RequestType request_type_;
  std::string additional_headers_;
  std::string post_data_mime_type_;
  std::string post_data_;
};

typedef CloudPrintURLFetcher::Delegate CloudPrintURLFetcherDelegate;

#endif  // CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_URL_FETCHER_H_

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_URL_FETCHER_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_URL_FETCHER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

namespace local_discovery {

const int kPrivetHTTPCodeInternalFailure = -1;

// Privet-specific URLFetcher adapter. Currently supports only the subset
// of HTTP features required by Privet for GCP 1.5
// (/privet/info and /privet/register).
class PrivetURLFetcher : public net::URLFetcherDelegate {
 public:
  enum ErrorType {
    JSON_PARSE_ERROR,
    URL_FETCH_ERROR,
    RESPONSE_CODE_ERROR,
    RETRY_ERROR,
    TOKEN_ERROR
  };

  typedef base::Callback<void(const std::string& /*token*/)> TokenCallback;

  class Delegate {
   public:
    virtual ~Delegate() {}

    // If you do not implement this method, you will always get a
    // TOKEN_ERROR error when your token is invalid.
    virtual void OnNeedPrivetToken(
        PrivetURLFetcher* fetcher,
        const TokenCallback& callback);
    virtual void OnError(PrivetURLFetcher* fetcher, ErrorType error) = 0;
    virtual void OnParsedJson(PrivetURLFetcher* fetcher,
                              const base::DictionaryValue* value,
                              bool has_error) = 0;
  };

  PrivetURLFetcher(
      const std::string& token,
      const GURL& url,
      net::URLFetcher::RequestType request_type,
      net::URLRequestContextGetter* request_context,
      Delegate* delegate);
  virtual ~PrivetURLFetcher();

  void DoNotRetryOnTransientError();

  void AllowEmptyPrivetToken();

  void Start();

  void SetUploadData(const std::string& upload_content_type,
                     const std::string& upload_data);

  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  const GURL& url() const { return url_fetcher_->GetOriginalURL(); }
  int response_code() const { return url_fetcher_->GetResponseCode(); }

 private:
  void Try();
  void ScheduleRetry(int timeout_seconds);
  bool PrivetErrorTransient(const std::string& error);
  void RequestTokenRefresh();
  void RefreshToken(const std::string& token);

  std::string privet_access_token_;
  GURL url_;
  net::URLFetcher::RequestType request_type_;
  scoped_refptr<net::URLRequestContextGetter> request_context_;
  Delegate* delegate_;

  bool do_not_retry_on_transient_error_;
  bool allow_empty_privet_token_;

  int tries_;
  std::string upload_data_;
  std::string upload_content_type_;
  scoped_ptr<net::URLFetcher> url_fetcher_;

  base::WeakPtrFactory<PrivetURLFetcher> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(PrivetURLFetcher);
};

class PrivetURLFetcherFactory {
 public:
  explicit PrivetURLFetcherFactory(
      net::URLRequestContextGetter* request_context);
  ~PrivetURLFetcherFactory();

  scoped_ptr<PrivetURLFetcher> CreateURLFetcher(
      const GURL& url,
      net::URLFetcher::RequestType request_type,
      PrivetURLFetcher::Delegate* delegate) const;

  void set_token(const std::string& token) { token_ = token; }
  const std::string& get_token() const { return token_; }

 private:
  scoped_refptr<net::URLRequestContextGetter> request_context_;
  std::string token_;

  DISALLOW_COPY_AND_ASSIGN(PrivetURLFetcherFactory);
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_URL_FETCHER_H_

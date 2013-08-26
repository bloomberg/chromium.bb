// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_CONFIRM_API_FLOW_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_CONFIRM_API_FLOW_H_

#include <string>

#include "chrome/browser/local_discovery/privet_http.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"

namespace local_discovery {

// API call flow for server-side communication with cloudprint for registration.
class PrivetConfirmApiCallFlow : public net::URLFetcherDelegate,
                                 public OAuth2TokenService::Consumer {
 public:
  // TODO(noamsml): Better error model for this class.
  enum Status {
    SUCCESS,
    ERROR_TOKEN,
    ERROR_NETWORK,
    ERROR_HTTP_CODE,
    ERROR_FROM_SERVER,
    ERROR_MALFORMED_RESPONSE
  };
  typedef base::Callback<void(Status /*success*/)> ResponseCallback;

  PrivetConfirmApiCallFlow(net::URLRequestContextGetter* request_context,
                           OAuth2TokenService* token_service_,
                           const GURL& automated_claim_url,
                           const ResponseCallback& callback);
  virtual ~PrivetConfirmApiCallFlow();

  void Start();

  // net::URLFetcherDelegate implementation:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // OAuth2TokenService::Consumer implementation:
  virtual void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                                 const std::string& access_token,
                                 const base::Time& expiration_time) OVERRIDE;
  virtual void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                                 const GoogleServiceAuthError& error) OVERRIDE;

 private:
  scoped_ptr<net::URLFetcher> url_fetcher_;
  scoped_ptr<OAuth2TokenService::Request> oauth_request_;
  scoped_refptr<net::URLRequestContextGetter> request_context_;
  OAuth2TokenService* token_service_;
  GURL automated_claim_url_;
  ResponseCallback callback_;
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_CONFIRM_API_FLOW_H_

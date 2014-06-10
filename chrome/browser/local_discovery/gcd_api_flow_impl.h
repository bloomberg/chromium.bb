// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_GCD_API_FLOW_IMPL_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_GCD_API_FLOW_IMPL_H_

#include "chrome/browser/local_discovery/gcd_api_flow.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace local_discovery {

namespace {
const char kCloudPrintOAuthHeaderFormat[] = "Authorization: Bearer %s";
}

class GCDApiFlowImpl : public GCDApiFlow,
                       public net::URLFetcherDelegate,
                       public OAuth2TokenService::Consumer {
 public:
  // Create an OAuth2-based confirmation.
  GCDApiFlowImpl(net::URLRequestContextGetter* request_context,
                 OAuth2TokenService* token_service,
                 const std::string& account_id);

  virtual ~GCDApiFlowImpl();

  virtual void Start(scoped_ptr<Request> request) OVERRIDE;

  // net::URLFetcherDelegate implementation:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // OAuth2TokenService::Consumer implementation:
  virtual void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                                 const std::string& access_token,
                                 const base::Time& expiration_time) OVERRIDE;
  virtual void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                                 const GoogleServiceAuthError& error) OVERRIDE;

 private:
  void CreateRequest(const GURL& url);

  scoped_ptr<net::URLFetcher> url_fetcher_;
  scoped_ptr<OAuth2TokenService::Request> oauth_request_;
  scoped_refptr<net::URLRequestContextGetter> request_context_;
  OAuth2TokenService* token_service_;
  std::string account_id_;
  scoped_ptr<Request> request_;

  DISALLOW_COPY_AND_ASSIGN(GCDApiFlowImpl);
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_GCD_API_FLOW_IMPL_H_

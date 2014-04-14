// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_BROWSER_HTTP_AUTH_HANDLER_DATA_REDUCTION_PROXY_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_BROWSER_HTTP_AUTH_HANDLER_DATA_REDUCTION_PROXY_H_

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "net/http/http_auth_handler.h"
#include "net/http/http_auth_handler_factory.h"

namespace data_reduction_proxy {

// Code for handling http SpdyProxy authentication.
class HttpAuthHandlerDataReductionProxy : public net::HttpAuthHandler {
 public:
  class Factory : public net::HttpAuthHandlerFactory {
   public:
    // Constructs a new spdyproxy handler factory which mints handlers that
    // respond to challenges only from the given |authorized_spdyproxy_origins|.
    explicit Factory(const std::vector<GURL>& authorized_spdyproxy_origins);
    virtual ~Factory();

    virtual int CreateAuthHandler(
        net::HttpAuthChallengeTokenizer* challenge,
        net::HttpAuth::Target target,
        const GURL& origin,
        CreateReason reason,
        int digest_nonce_count,
        const net::BoundNetLog& net_log,
        scoped_ptr<HttpAuthHandler>* handler) OVERRIDE;

   private:
    // The origins for which Chrome will respond to SpdyProxy auth challenges.
    std::vector<GURL> authorized_spdyproxy_origins_;
  };

  // Constructs a new spdyproxy handler.
  HttpAuthHandlerDataReductionProxy() {}

  // Overrides from net::HttpAuthHandler.
  virtual net::HttpAuth::AuthorizationResult HandleAnotherChallenge(
      net::HttpAuthChallengeTokenizer* challenge) OVERRIDE;
  virtual bool NeedsIdentity() OVERRIDE;
  virtual bool AllowsDefaultCredentials() OVERRIDE;
  virtual bool AllowsExplicitCredentials() OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(HttpAuthHandlerDataReductionProxyTest,
                           ParseChallenge);

  virtual ~HttpAuthHandlerDataReductionProxy();

  virtual bool Init(net::HttpAuthChallengeTokenizer* challenge) OVERRIDE;

  virtual int GenerateAuthTokenImpl(const net::AuthCredentials* credentials,
                                    const net::HttpRequestInfo* request,
                                    const net::CompletionCallback& callback,
                                    std::string* auth_token) OVERRIDE;

  bool ParseChallenge(net::HttpAuthChallengeTokenizer* challenge);

  bool ParseChallengeProperty(const std::string& name,
                              const std::string& value);

  // Proxy server token, encoded as UTF-8.
  std::string ps_token_;

  DISALLOW_COPY_AND_ASSIGN(HttpAuthHandlerDataReductionProxy);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_BROWSER_HTTP_AUTH_HANDLER_DATA_REDUCTION_PROXY_H_

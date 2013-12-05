// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_FAKE_GAIA_H_
#define GOOGLE_APIS_GAIA_FAKE_GAIA_H_

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class DictionaryValue;
}

namespace net {
namespace test_server {
class BasicHttpResponse;
struct HttpRequest;
class HttpResponse;
}
}

// This is a test helper that implements a fake GAIA service for use in browser
// tests. It's mainly intended for use with EmbeddedTestServer, for which it can
// be registered as an additional request handler.
class FakeGaia {
 public:
  typedef std::set<std::string> ScopeSet;

  // Access token details used for token minting and the token info endpoint.
  struct AccessTokenInfo {
    AccessTokenInfo();
    ~AccessTokenInfo();

    std::string token;
    std::string issued_to;
    std::string audience;
    std::string user_id;
    ScopeSet scopes;
    int expires_in;
    std::string email;
  };

  FakeGaia();
  ~FakeGaia();

  // Handles a request and returns a response if the request was recognized as a
  // GAIA request. Note that this respects the switches::kGaiaUrl and friends so
  // that this can used with EmbeddedTestServer::RegisterRequestHandler().
  scoped_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request);

  // Configures an OAuth2 token that'll be returned when a client requests an
  // access token for the given auth token, which can be a refresh token or an
  // login-scoped access token for the token minting endpoint. Note that the
  // scope and audience requested by the client need to match the token_info.
  void IssueOAuthToken(const std::string& auth_token,
                       const AccessTokenInfo& token_info);

 private:
  typedef std::multimap<std::string, AccessTokenInfo> AccessTokenInfoMap;

  // Formats a JSON response with the data in |response_dict|.
  void FormatJSONResponse(const base::DictionaryValue& response_dict,
                          net::test_server::BasicHttpResponse* http_response);

  // Returns the access token associated with |auth_token| that matches the
  // given |client_id| and |scope_string|. If |scope_string| is empty, the first
  // token satisfying the other criteria is returned. Returns NULL if no token
  // matches.
  const AccessTokenInfo* GetAccessTokenInfo(const std::string& auth_token,
                                            const std::string& client_id,
                                            const std::string& scope_string)
      const;

  // Extracts the parameter named |key| from |query| and places it in |value|.
  // Returns false if no parameter is found.
  static bool GetQueryParameter(const std::string& query,
                                const std::string& key,
                                std::string* value);

  AccessTokenInfoMap access_token_info_map_;
  std::string service_login_response_;

  DISALLOW_COPY_AND_ASSIGN(FakeGaia);
};

#endif  // GOOGLE_APIS_GAIA_FAKE_GAIA_H_

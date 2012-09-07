// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_GAIA_OAUTH_CLIENT_H_
#define GOOGLE_APIS_GAIA_GAIA_OAUTH_CLIENT_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/message_loop_proxy.h"

namespace net {
class URLRequestContextGetter;
}

// A helper class to get and refresh OAuth tokens given an authorization code.
namespace gaia {

static const char kGaiaOAuth2Url[] =
    "https://accounts.google.com/o/oauth2/token";

struct OAuthClientInfo {
  std::string client_id;
  std::string client_secret;
};

class GaiaOAuthClient {
 public:
  class Delegate {
   public:
    // Invoked on a successful response to the GetTokensFromAuthCode request.
    virtual void OnGetTokensResponse(const std::string& refresh_token,
                                     const std::string& access_token,
                                     int expires_in_seconds) = 0;
    // Invoked on a successful response to the RefreshToken request.
    virtual void OnRefreshTokenResponse(const std::string& access_token,
                                        int expires_in_seconds) = 0;
    // Invoked when there is an OAuth error with one of the requests.
    virtual void OnOAuthError() = 0;
    // Invoked when there is a network error or upon receiving an invalid
    // response. This is invoked when the maximum number of retries have been
    // exhausted. If max_retries is -1, this is never invoked.
    virtual void OnNetworkError(int response_code) = 0;

   protected:
    virtual ~Delegate() {}
  };
  GaiaOAuthClient(const std::string& gaia_url,
                  net::URLRequestContextGetter* context_getter);
  ~GaiaOAuthClient();

  // In the below methods, |max_retries| specifies the maximum number of times
  // we should retry on a network error in invalid response. This does not
  // apply in the case of an OAuth error (i.e. there was something wrong with
  // the input arguments). Setting |max_retries| to -1 implies infinite retries.
  void GetTokensFromAuthCode(const OAuthClientInfo& oauth_client_info,
                             const std::string& auth_code,
                             int max_retries,
                             Delegate* delegate);
  void RefreshToken(const OAuthClientInfo& oauth_client_info,
                    const std::string& refresh_token,
                    int max_retries,
                    Delegate* delegate);

 private:
  // The guts of the implementation live in this class.
  class Core;
  scoped_refptr<Core> core_;
  DISALLOW_COPY_AND_ASSIGN(GaiaOAuthClient);
};
}

#endif  // GOOGLE_APIS_GAIA_GAIA_OAUTH_CLIENT_H_

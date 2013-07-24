// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_GCP20_PROTOTYPE_CLOUD_REQUESTER_H_
#define CLOUD_PRINT_GCP20_PROTOTYPE_CLOUD_REQUESTER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"

typedef base::Callback<void(const std::string&)> ParserCallback;

class CloudPrintURLRequestContextGetter;

extern const char kCloudPrintUrl[];

// Class for requesting CloudPrint server and parsing responses.
class CloudPrintRequester : public base::SupportsWeakPtr<CloudPrintRequester>,
                            public net::URLFetcherDelegate,
                            public gaia::GaiaOAuthClient::Delegate {
 public:
  class Delegate {
   public:
    Delegate() {}
    virtual ~Delegate() {}

    // Invoked when server respond for registration-start query and response is
    // successfully parsed.
    virtual void OnRegistrationStartResponseParsed(
        const std::string& registration_token,
        const std::string& complete_invite_url,
        const std::string& device_id) = 0;

    // Invoked when server respond for registration-getAuthCode query and
    // response is successfully parsed.
    virtual void OnGetAuthCodeResponseParsed(
        const std::string& refresh_token) = 0;

    // Invoked when server respond with |"success" = false| or we cannot parse
    // response.
    virtual void OnRegistrationError(const std::string& description) = 0;
  };

  // Creates and initializes objects.
  CloudPrintRequester(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                      Delegate* delegate);

  // Destroys the object.
  virtual ~CloudPrintRequester();

  // Creates query to server for starting registration.
  bool StartRegistration(const std::string& proxy_id,
                         const std::string& device_name,
                         const std::string& user,
                         const std::string& cdd);

  // Creates request for completing registration and receiving refresh token.
  bool CompleteRegistration();

 private:
  // net::URLFetcherDelegate
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // gaia::GaiaOAuthClient::Delegate
  virtual void OnGetTokensResponse(const std::string& refresh_token,
                                   const std::string& access_token,
                                   int expires_in_seconds) OVERRIDE;
  virtual void OnRefreshTokenResponse(const std::string& access_token,
                                      int expires_in_seconds) OVERRIDE;
  virtual void OnOAuthError() OVERRIDE;
  virtual void OnNetworkError(int response_code) OVERRIDE;

  // Creates request with given |url| and |method|. When response is received
  // callback is called.
  // TODO(maksymb): Add timeout control for request.
  bool CreateRequest(const GURL& url,
                     net::URLFetcher::RequestType method,
                     const ParserCallback& callback) WARN_UNUSED_RESULT;

  // Parses register-start server response.
  void ParseRegisterStartResponse(const std::string& response);

  // Parses register-complete server response. Initializes gaia (OAuth client)
  // and receives refresh token.
  void ParseRegisterCompleteResponse(const std::string& response);

  // Fetcher contains |NULL| if no server response is awaiting. Otherwise wait
  // until URLFetchComplete will be called and close connection.
  scoped_ptr<net::URLFetcher> fetcher_;

  // Callback for parsing server response.
  ParserCallback parse_response_callback_;

  // Privet context getter.
  scoped_refptr<CloudPrintURLRequestContextGetter> context_getter_;

  // URL for completing registration and receiving OAuth account.
  std::string polling_url_;

  // Last valid access_token.
  std::string access_token_;

  // OAuth client information (client_id, client_secret, etc).
  gaia::OAuthClientInfo oauth_client_info_;

  // OAuth client.
  scoped_ptr<gaia::GaiaOAuthClient> gaia_;

  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(CloudPrintRequester);
};

#endif  // CLOUD_PRINT_GCP20_PROTOTYPE_CLOUD_REQUESTER_H_


// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_AUTH_H_
#define CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_AUTH_H_
#pragma once

#include <string>

#include "base/values.h"
#include "chrome/common/net/gaia/gaia_oauth_client.h"
#include "chrome/service/cloud_print/cloud_print_url_fetcher.h"
#include "googleurl/src/gurl.h"

// CloudPrintAuth is a class to handle login, token refresh, and other
// authentication tasks for Cloud Print.
// CloudPrintAuth will create new robot account for this proxy if needed.
// CloudPrintAuth will obtain new OAuth token.
// CloudPrintAuth will schedule periodic OAuth token refresh
// It is running in the same thread as CloudPrintProxyBackend::Core.
class CloudPrintAuth
    : public base::RefCountedThreadSafe<CloudPrintAuth>,
      public CloudPrintURLFetcherDelegate,
      public gaia::GaiaOAuthClient::Delegate {
 public:
  class Client {
   public:
    virtual void OnAuthenticationComplete(
        const std::string& access_token,
        const std::string& robot_oauth_refresh_token,
        const std::string& robot_email,
        const std::string& user_email) = 0;
    virtual void OnInvalidCredentials() = 0;
   protected:
     virtual ~Client() {}
  };

  CloudPrintAuth(Client* client,
                 const GURL& cloud_print_server_url,
                 const base::DictionaryValue* print_sys_settings,
                 const gaia::OAuthClientInfo& oauth_client_info,
                 const std::string& proxy_id);
  virtual ~CloudPrintAuth();

  // Note:
  //
  // The Authenticate* methods are the various entry points from
  // CloudPrintProxyBackend::Core. It calls us on a dedicated thread to
  // actually perform synchronous (and potentially blocking) operations.
  //
  // When we are passed in an LSID we authenticate using that
  // and retrieve new auth tokens.
  void AuthenticateWithLsid(const std::string& lsid,
                            const std::string& last_robot_refresh_token,
                            const std::string& last_robot_email,
                            const std::string& last_user_email);

  void AuthenticateWithToken(const std::string cloud_print_token);
  void AuthenticateWithRobotToken(const std::string& robot_oauth_refresh_token,
                                  const std::string& robot_email);
  void AuthenticateWithRobotAuthCode(const std::string& robot_oauth_auth_code,
                                     const std::string& robot_email);

  void RefreshAccessToken();

  // gaia::GaiaOAuthClient::Delegate implementation.
  virtual void OnGetTokensResponse(const std::string& refresh_token,
                                   const std::string& access_token,
                                   int expires_in_seconds) OVERRIDE;
  virtual void OnRefreshTokenResponse(const std::string& access_token,
                                      int expires_in_seconds) OVERRIDE;
  virtual void OnOAuthError() OVERRIDE;
  virtual void OnNetworkError(int response_code) OVERRIDE;

  // CloudPrintURLFetcher::Delegate implementation.
  virtual CloudPrintURLFetcher::ResponseAction HandleJSONData(
      const content::URLFetcher* source,
      const GURL& url,
      base::DictionaryValue* json_data,
      bool succeeded) OVERRIDE;
  virtual CloudPrintURLFetcher::ResponseAction OnRequestAuthError() OVERRIDE;
  virtual std::string GetAuthHeader() OVERRIDE;

 private:
  Client* client_;
  gaia::OAuthClientInfo oauth_client_info_;
  scoped_ptr<gaia::GaiaOAuthClient> oauth_client_;
  scoped_ptr<DictionaryValue> print_system_settings_;

  // The CloudPrintURLFetcher instance for the current request.
  scoped_refptr<CloudPrintURLFetcher> request_;

  GURL cloud_print_server_url_;
  // Proxy id, need to send to the cloud print server to find and update
  // necessary printers during the migration process.
  const std::string& proxy_id_;
  // The OAuth2 refresh token for the robot.
  std::string refresh_token_;
  // The email address of the user. This is only used during initial
  // authentication with an LSID. This is only used for storing in prefs for
  // display purposes.
  std::string user_email_;
  // The email address of the robot account.
  std::string robot_email_;
  // client login token used to authenticate request to cloud print server to
  // get the robot account.
  std::string client_login_token_;

  DISALLOW_COPY_AND_ASSIGN(CloudPrintAuth);
};

#endif  // CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_AUTH_H_


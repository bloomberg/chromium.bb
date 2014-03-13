// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_OAUTH2_TOKEN_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_OAUTH2_TOKEN_SERVICE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/url_request/url_request_context_getter.h"

namespace gaia {
class GaiaOAuthClient;
}

namespace net {
class URLRequestContextGetter;
}

class PrefRegistrySimple;
class PrefService;

namespace chromeos {

// DeviceOAuth2TokenService retrieves OAuth2 access tokens for a given
// set of scopes using the device-level OAuth2 any-api refresh token
// obtained during enterprise device enrollment.
//
// See |OAuth2TokenService| for usage details.
//
// When using DeviceOAuth2TokenSerivce, a value of |GetRobotAccountId| should
// be used in places where API expects |account_id|.
//
// Note that requests must be made from the UI thread.
class DeviceOAuth2TokenService : public OAuth2TokenService,
                                 public gaia::GaiaOAuthClient::Delegate {
 public:
  typedef base::Callback<void(bool)> StatusCallback;

  // Persist the given refresh token on the device. Overwrites any previous
  // value. Should only be called during initial device setup. Signals
  // completion via the given callback, passing true if the operation succeeded.
  void SetAndSaveRefreshToken(const std::string& refresh_token,
                              const StatusCallback& callback);

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Implementation of OAuth2TokenService.
  virtual bool RefreshTokenIsAvailable(const std::string& account_id)
      const OVERRIDE;

  // Pull the robot account ID from device policy.
  virtual std::string GetRobotAccountId() const;

  // gaia::GaiaOAuthClient::Delegate implementation.
  virtual void OnRefreshTokenResponse(const std::string& access_token,
                                      int expires_in_seconds) OVERRIDE;
  virtual void OnGetTokenInfoResponse(
      scoped_ptr<base::DictionaryValue> token_info) OVERRIDE;
  virtual void OnOAuthError() OVERRIDE;
  virtual void OnNetworkError(int response_code) OVERRIDE;

 protected:
  // Implementation of OAuth2TokenService.
  virtual net::URLRequestContextGetter* GetRequestContext() OVERRIDE;
  virtual void FetchOAuth2Token(RequestImpl* request,
                                const std::string& account_id,
                                net::URLRequestContextGetter* getter,
                                const std::string& client_id,
                                const std::string& client_secret,
                                const ScopeSet& scopes) OVERRIDE;
  virtual OAuth2AccessTokenFetcher* CreateAccessTokenFetcher(
      const std::string& account_id,
      net::URLRequestContextGetter* getter,
      OAuth2AccessTokenConsumer* consumer) OVERRIDE;

 private:
  struct PendingRequest;
  friend class DeviceOAuth2TokenServiceFactory;
  friend class DeviceOAuth2TokenServiceTest;

  // Describes the operational state of this object.
  enum State {
    // Pending system salt / refresh token load.
    STATE_LOADING,
    // No token available.
    STATE_NO_TOKEN,
    // System salt loaded, validation not started yet.
    STATE_VALIDATION_PENDING,
    // Refresh token validation underway.
    STATE_VALIDATION_STARTED,
    // Token validation failed.
    STATE_TOKEN_INVALID,
    // Refresh token is valid.
    STATE_TOKEN_VALID,
  };

  // Use DeviceOAuth2TokenServiceFactory to get an instance of this class.
  // Ownership of |token_encryptor| will be taken.
  explicit DeviceOAuth2TokenService(net::URLRequestContextGetter* getter,
                                    PrefService* local_state);
  virtual ~DeviceOAuth2TokenService();

  // Returns the refresh token for account_id.
  std::string GetRefreshToken(const std::string& account_id) const;

  // Handles completion of the system salt input.
  void DidGetSystemSalt(const std::string& system_salt);

  // Checks whether |gaia_robot_id| matches the expected account ID indicated in
  // device settings.
  void CheckRobotAccountId(const std::string& gaia_robot_id);

  // Encrypts and saves the refresh token. Should only be called when the system
  // salt is available.
  void EncryptAndSaveToken();

  // Starts the token validation flow, i.e. token info fetch.
  void StartValidation();

  // Flushes |pending_requests_|, indicating the specified result.
  void FlushPendingRequests(bool token_is_valid,
                            GoogleServiceAuthError::State error);

  // Flushes |token_save_callbacks_|, indicating the specified result.
  void FlushTokenSaveCallbacks(bool result);

  // Signals failure on the specified request, passing |error| as the reason.
  void FailRequest(RequestImpl* request, GoogleServiceAuthError::State error);

  // Dependencies.
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;
  PrefService* local_state_;

  // Current operational state.
  State state_;

  // Token save callbacks waiting to be completed.
  std::vector<StatusCallback> token_save_callbacks_;

  // Currently open requests that are waiting while loading the system salt or
  // validating the token.
  std::vector<PendingRequest*> pending_requests_;

  // The system salt for encrypting and decrypting the refresh token.
  std::string system_salt_;

  int max_refresh_token_validation_retries_;

  // Cache the decrypted refresh token, so we only decrypt once.
  std::string refresh_token_;

  scoped_ptr<gaia::GaiaOAuthClient> gaia_oauth_client_;

  base::WeakPtrFactory<DeviceOAuth2TokenService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DeviceOAuth2TokenService);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_OAUTH2_TOKEN_SERVICE_H_

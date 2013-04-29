// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_OAUTH2_TOKEN_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_OAUTH2_TOKEN_SERVICE_H_

#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/signin/oauth2_token_service.h"
#include "net/url_request/url_request_context_getter.h"

namespace net {
class URLRequestContextGetter;
}

class GoogleServiceAuthError;
class PrefRegistrySimple;
class PrefService;
class Profile;

namespace chromeos {

// DeviceOAuth2TokenService retrieves OAuth2 access tokens for a given
// set of scopes using the device-level OAuth2 any-api refresh token
// obtained during enterprise device enrollment.
//
// See |OAuth2TokenService| for usage details.
//
// Note that requests must be made from the UI thread.
class DeviceOAuth2TokenService : public OAuth2TokenService {
 public:
  // Persist the given refresh token on the device.  Overwrites any previous
  // value.  Should only be called during initial device setup.
  void SetAndSaveRefreshToken(const std::string& refresh_token);

  static void RegisterPrefs(PrefRegistrySimple* registry);

  virtual std::string GetRefreshToken() OVERRIDE;

 private:
  friend class DeviceOAuth2TokenServiceFactory;
  FRIEND_TEST_ALL_PREFIXES(DeviceOAuth2TokenServiceTest, SaveEncryptedToken);

  // Use DeviceOAuth2TokenServiceFactory to get an instance of this class.
  explicit DeviceOAuth2TokenService(net::URLRequestContextGetter* getter,
                                    PrefService* local_state);
  virtual ~DeviceOAuth2TokenService();

  // Cache the decrypted refresh token, so we only decrypt once.
  std::string refresh_token_;
  PrefService* local_state_;
  DISALLOW_COPY_AND_ASSIGN(DeviceOAuth2TokenService);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_OAUTH2_TOKEN_SERVICE_H_

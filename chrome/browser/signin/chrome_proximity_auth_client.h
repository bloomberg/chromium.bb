// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_CHROME_PROXIMITY_AUTH_CLIENT_H_
#define CHROME_BROWSER_SIGNIN_CHROME_PROXIMITY_AUTH_CLIENT_H_

#include "base/macros.h"
#include "components/proximity_auth/proximity_auth_client.h"

class PrefService;
class Profile;
class EasyUnlockServiceRegular;

// A Chrome-specific implementation of the ProximityAuthClient interface.
// There is one |ChromeProximityAuthClient| per |Profile|.
class ChromeProximityAuthClient : public proximity_auth::ProximityAuthClient {
 public:
  explicit ChromeProximityAuthClient(Profile* profile);
  ~ChromeProximityAuthClient() override;

  // proximity_auth::ProximityAuthClient:
  std::string GetAuthenticatedUsername() const override;
  void UpdateScreenlockState(proximity_auth::ScreenlockState state) override;
  void FinalizeUnlock(bool success) override;
  PrefService* GetPrefService() override;
  scoped_ptr<proximity_auth::SecureMessageDelegate>
  CreateSecureMessageDelegate() override;
  scoped_ptr<proximity_auth::CryptAuthClientFactory>
  CreateCryptAuthClientFactory() override;
  cryptauth::DeviceClassifier GetDeviceClassifier() override;
  std::string GetAccountId() override;
  proximity_auth::CryptAuthEnrollmentManager* GetCryptAuthEnrollmentManager()
      override;
  proximity_auth::CryptAuthDeviceManager* GetCryptAuthDeviceManager() override;
  void FinalizeSignin(const std::string& secret) override;

 private:
  Profile* const profile_;

  DISALLOW_COPY_AND_ASSIGN(ChromeProximityAuthClient);
};

#endif  // CHROME_BROWSER_SIGNIN_CHROME_PROXIMITY_AUTH_CLIENT_H_

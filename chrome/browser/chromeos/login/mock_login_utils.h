// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_LOGIN_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_LOGIN_UTILS_H_

#include <string>

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/browser/chromeos/login/login_display_host.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"

class PrefService;
class Profile;

namespace chromeos {

class LoginStatusConsumer;

class MockLoginUtils : public LoginUtils {
 public:
  MockLoginUtils();
  virtual ~MockLoginUtils();

  MOCK_METHOD2(DoBrowserLaunch, void(Profile*, LoginDisplayHost*));
  MOCK_METHOD7(PrepareProfile,
               void(const std::string&, const std::string&, const std::string&,
                    bool, bool, bool,
                    LoginUtils::Delegate*));
  MOCK_METHOD1(DelegateDeleted, void(LoginUtils::Delegate*));
  MOCK_METHOD1(CompleteOffTheRecordLogin, void(const GURL&));
  MOCK_METHOD1(SetFirstLoginPrefs, void(PrefService*));
  MOCK_METHOD1(CreateAuthenticator,
               scoped_refptr<Authenticator>(LoginStatusConsumer*));
  MOCK_METHOD0(PrewarmAuthentication, void(void));
  MOCK_METHOD1(RestoreAuthenticationSession, void(Profile*));
  MOCK_METHOD1(StartTokenServices, void(Profile*));
  MOCK_METHOD2(StartSignedInServices,
               void(Profile*, const GaiaAuthConsumer::ClientLoginResult&));
  MOCK_METHOD2(TransferDefaultCookies, void(Profile*, Profile*));
  MOCK_METHOD2(TransferDefaultAuthCache, void(Profile*, Profile*));
  MOCK_METHOD0(StopBackgroundFetchers, void(void));
  MOCK_METHOD3(GetOffTheRecordCommandLine,
               std::string(const GURL&, const CommandLine&, CommandLine*));
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_LOGIN_UTILS_H_

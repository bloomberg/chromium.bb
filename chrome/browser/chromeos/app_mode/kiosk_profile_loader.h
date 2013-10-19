// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_PROFILE_LOADER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_PROFILE_LOADER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/chromeos/login/login_performer.h"
#include "chrome/browser/chromeos/login/login_utils.h"

class Profile;

namespace chromeos {

class KioskAppManager;

// KioskProfileLoader loads a special profile for a given app. It first
// attempts to login for the app's generated user id. If the login is
// successful, it prepares app profile then calls the delegate.
class KioskProfileLoader : public LoginPerformer::Delegate,
                           public LoginUtils::Delegate {
 public:
  class Delegate {
   public:
    virtual void OnProfileLoaded(Profile* profile) = 0;
    virtual void OnProfileLoadFailed(KioskAppLaunchError::Error error) = 0;

   protected:
    virtual ~Delegate() {}
  };

  KioskProfileLoader(KioskAppManager* kiosk_app_manager,
                     const std::string& app_id,
                     Delegate* delegate);

  virtual ~KioskProfileLoader();

  // Starts profile load. Calls delegate on success or failure.
  void Start();

 private:
  class CryptohomedChecker;

  void LoginAsKioskAccount();
  void ReportLaunchResult(KioskAppLaunchError::Error error);

  // LoginPerformer::Delegate overrides
  virtual void OnLoginSuccess(const UserContext& user_context) OVERRIDE;
  virtual void OnLoginFailure(const LoginFailure& error) OVERRIDE;
  virtual void WhiteListCheckFailed(const std::string& email) OVERRIDE;
  virtual void PolicyLoadFailed() OVERRIDE;
  virtual void OnOnlineChecked(
      const std::string& email, bool success) OVERRIDE;

  // LoginUtils::Delegate implementation:
  virtual void OnProfilePrepared(Profile* profile) OVERRIDE;

  KioskAppManager* kiosk_app_manager_;
  const std::string app_id_;
  std::string user_id_;
  Delegate* delegate_;
  scoped_ptr<CryptohomedChecker> cryptohomed_checker_;
  scoped_ptr<LoginPerformer> login_performer_;

  DISALLOW_COPY_AND_ASSIGN(KioskProfileLoader);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_PROFILE_LOADER_H_

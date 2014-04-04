// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PROFILES_PROFILE_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_PROFILES_PROFILE_HELPER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/chromeos/login/oauth2_login_manager.h"
#include "chrome/browser/chromeos/login/user_manager.h"

class Profile;

namespace base {
class FilePath;
}

namespace chromeos {

// This helper class is used on Chrome OS to keep track of currently
// active user profile.
// Whenever active user is changed (either add another user into session or
// switch between users), ActiveUserHashChanged() will be called thus
// internal state |active_user_id_hash_| will be updated.
// Typical use cases for using this class:
// 1. Get "signin profile" which is a special type of profile that is only used
//    during signin flow: GetSigninProfile()
// 2. Get profile dir of an active user, used by ProfileManager:
//    GetActiveUserProfileDir()
// 3. Get mapping from user_id_hash to Profile instance/profile path etc.
class ProfileHelper : public BrowsingDataRemover::Observer,
                      public OAuth2LoginManager::Observer,
                      public UserManager::UserSessionStateObserver {
 public:
  ProfileHelper();
  virtual ~ProfileHelper();

  // Returns Profile instance that corresponds to |user_id_hash|.
  static Profile* GetProfileByUserIdHash(const std::string& user_id_hash);

  // Returns profile dir that corresponds to a --login-profile cmd line switch.
  static base::FilePath GetProfileDirByLegacyLoginProfileSwitch();

  // Returns profile path that corresponds to a given |user_id_hash|.
  static base::FilePath GetProfilePathByUserIdHash(
      const std::string& user_id_hash);

  // Returns the path that corresponds to the sign-in profile.
  static base::FilePath GetSigninProfileDir();

  // Returns OffTheRecord profile for use during signing phase.
  static Profile* GetSigninProfile();

  // Returns user_id hash for |profile| instance or empty string if hash
  // could not be extracted from |profile|.
  static std::string GetUserIdHashFromProfile(Profile* profile);

  // Returns user profile dir in a format [u-user_id_hash].
  static base::FilePath GetUserProfileDir(const std::string& user_id_hash);

  // Returns true if |profile| is the signin Profile. This can be used during
  // construction of the signin Profile to determine if that Profile is the
  // signin Profile.
  static bool IsSigninProfile(Profile* profile);

  // Returns true when |profile| corresponds to owner's profile.
  static bool IsOwnerProfile(Profile* profile);

  // Initialize a bunch of services that are tied to a browser profile.
  // TODO(dzhioev): Investigate whether or not this method is needed.
  void ProfileStartup(Profile* profile, bool process_startup);

  // Returns active user profile dir in a format [u-$hash].
  base::FilePath GetActiveUserProfileDir();

  // Should called once after UserManager instance has been created.
  void Initialize();

  // Returns hash for active user ID which is used to identify that user profile
  // on Chrome OS.
  std::string active_user_id_hash() { return active_user_id_hash_; }

  // Clears site data (cookies, history, etc) for signin profile.
  // Callback can be empty. Not thread-safe.
  void ClearSigninProfile(const base::Closure& on_clear_callback);

 private:
  friend class ProfileHelperTest;
  friend class ProfileListChromeOSTest;

  // BrowsingDataRemover::Observer implementation:
  virtual void OnBrowsingDataRemoverDone() OVERRIDE;

  // UserManager::Observer overrides.
  virtual void OnSessionRestoreStateChanged(
      Profile* user_profile,
      OAuth2LoginManager::SessionRestoreState state) OVERRIDE;

  // UserManager::UserSessionStateObserver implementation:
  virtual void ActiveUserHashChanged(const std::string& hash) OVERRIDE;

  // Identifies path to active user profile on Chrome OS.
  std::string active_user_id_hash_;

  // True if signin profile clearing now.
  bool signin_profile_clear_requested_;

  // List of callbacks called after signin profile clearance.
  std::vector<base::Closure> on_clear_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(ProfileHelper);
};

} // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PROFILES_PROFILE_HELPER_H_

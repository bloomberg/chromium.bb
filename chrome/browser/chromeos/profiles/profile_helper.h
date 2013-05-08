// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PROFILES_PROFILE_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_PROFILES_PROFILE_HELPER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/chromeos/login/user_manager.h"

class Profile;

namespace chromeos {

class ProfileHelper : public BrowsingDataRemover::Observer,
                      public UserManager::UserSessionStateObserver {
 public:
  ProfileHelper();
  virtual ~ProfileHelper();

  // Returns Profile instance that corresponds to |user_id_hash|.
  static Profile* GetProfileByUserIdHash(const std::string& user_id_hash);

  // Returns OffTheRecord profile for use during signing phase.
  static Profile* GetSigninProfile();

  // Returns true if |profile| is the signin Profile. This can be used during
  // construction of the signin Profile to determine if that Profile is the
  // signin Profile.
  static bool IsSigninProfile(Profile* profile);

  // Initialize a bunch of services that are tied to a browser profile.
  // TODO(dzhioev): Investigate whether or not this method is needed.
  static void ProfileStartup(Profile* profile, bool process_startup);

  // Should called once after UserManager instance has been created.
  void Initialize();

  // Returns hash for active user ID which is used to identify that user profile
  // on Chrome OS.
  std::string active_user_id_hash() { return active_user_id_hash_; }

  // Clears site data (cookies, history, etc) for signin profile.
  // Callback can be empty. Not thread-safe.
  void ClearSigninProfile(const base::Closure& on_clear_callback);

 private:
  // UserManager::UserSessionStateObserver implementation:
  virtual void ActiveUserHashChanged(const std::string& hash) OVERRIDE;

  // BrowsingDataRemover::Observer implementation:
  virtual void OnBrowsingDataRemoverDone() OVERRIDE;

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


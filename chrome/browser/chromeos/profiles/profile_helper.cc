// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/profiles/profile_helper.h"

#include "base/callback.h"
#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/chromeos/login/oauth2_login_manager_factory.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/chromeos_switches.h"

namespace chromeos {

namespace {

bool ShouldAddProfileDirPrefix(const std::string& user_id_hash) {
  // Do not add profile dir prefix for legacy profile dir and test
  // user profile. The reason of not adding prefix for test user profile
  // is to keep the promise that TestingProfile::kTestUserProfileDir and
  // chrome::kTestUserProfileDir are always in sync. Otherwise,
  // TestingProfile::kTestUserProfileDir needs to be dynamically calculated
  // based on whether multi profile is enabled or not.
  return user_id_hash != chrome::kLegacyProfileDir &&
      user_id_hash != chrome::kTestUserProfileDir;
}

}  // anonymous namespace

////////////////////////////////////////////////////////////////////////////////
// ProfileHelper, public

ProfileHelper::ProfileHelper()
  : signin_profile_clear_requested_(false) {
}

ProfileHelper::~ProfileHelper() {
  // Checking whether UserManager is initialized covers case
  // when ScopedTestUserManager is used.
  if (UserManager::IsInitialized())
    UserManager::Get()->RemoveSessionStateObserver(this);
}

// static
Profile* ProfileHelper::GetProfileByUserIdHash(
    const std::string& user_id_hash) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  return profile_manager->GetProfile(GetProfilePathByUserIdHash(user_id_hash));
}

// static
base::FilePath ProfileHelper::GetProfilePathByUserIdHash(
    const std::string& user_id_hash) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath profile_path = profile_manager->user_data_dir();

  return profile_path.Append(GetUserProfileDir(user_id_hash));
}

// static
base::FilePath ProfileHelper::GetProfileDirByLegacyLoginProfileSwitch() {
  const std::string login_profile_value =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          chromeos::switches::kLoginProfile);
  return ProfileHelper::GetUserProfileDir(login_profile_value);
}

// static
base::FilePath ProfileHelper::GetSigninProfileDir() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath user_data_dir = profile_manager->user_data_dir();
  return user_data_dir.AppendASCII(chrome::kInitialProfile);
}

// static
Profile* ProfileHelper::GetSigninProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  return profile_manager->GetProfile(GetSigninProfileDir())->
      GetOffTheRecordProfile();
}

// static
std::string ProfileHelper::GetUserIdHashFromProfile(Profile* profile) {
  if (!profile)
    return std::string();

  std::string profile_dir = profile->GetPath().BaseName().value();

  // Don't strip prefix if the dir is not supposed to be prefixed.
  if (!ShouldAddProfileDirPrefix(profile_dir))
    return profile_dir;

  // Check that profile directory starts with the correct prefix.
  std::string prefix(chrome::kProfileDirPrefix);
  if (profile_dir.find(prefix) != 0) {
    // This happens when creating a TestingProfile in browser tests.
    return std::string();
  }

  return profile_dir.substr(prefix.length(),
                            profile_dir.length() - prefix.length());
}

// static
base::FilePath ProfileHelper::GetUserProfileDir(
    const std::string& user_id_hash) {
  DCHECK(!user_id_hash.empty());
  return ShouldAddProfileDirPrefix(user_id_hash)
             ? base::FilePath(chrome::kProfileDirPrefix + user_id_hash)
             : base::FilePath(user_id_hash);
}

// static
bool ProfileHelper::IsSigninProfile(Profile* profile) {
  return profile->GetPath().BaseName().value() == chrome::kInitialProfile;
}

// static
bool ProfileHelper::IsOwnerProfile(Profile* profile) {
  if (!profile)
    return false;
  chromeos::UserManager* manager = chromeos::UserManager::Get();
  chromeos::User* user = manager->GetUserByProfile(profile);
  if (!user)
    return false;
  return user->email() == manager->GetOwnerEmail();
}

void ProfileHelper::ProfileStartup(Profile* profile, bool process_startup) {
  // Initialize Chrome OS preferences like touch pad sensitivity. For the
  // preferences to work in the guest mode, the initialization has to be
  // done after |profile| is switched to the incognito profile (which
  // is actually GuestSessionProfile in the guest mode). See the
  // GetOffTheRecordProfile() call above.
  profile->InitChromeOSPreferences();

  // Add observer so we can see when the first profile's session restore is
  // completed. After that, we won't need the default profile anymore.
  if (!IsSigninProfile(profile) &&
      UserManager::Get()->IsLoggedInAsRegularUser() &&
      !UserManager::Get()->IsLoggedInAsStub()) {
    chromeos::OAuth2LoginManager* login_manager =
        chromeos::OAuth2LoginManagerFactory::GetInstance()->GetForProfile(
            profile);
    if (login_manager)
      login_manager->AddObserver(this);
  }
}

base::FilePath ProfileHelper::GetActiveUserProfileDir() {
  return ProfileHelper::GetUserProfileDir(active_user_id_hash_);
}

void ProfileHelper::Initialize() {
  UserManager::Get()->AddSessionStateObserver(this);
}

void ProfileHelper::ClearSigninProfile(const base::Closure& on_clear_callback) {
  on_clear_callbacks_.push_back(on_clear_callback);
  if (signin_profile_clear_requested_)
    return;
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // Check if signin profile was loaded.
  if (!profile_manager->GetProfileByPath(GetSigninProfileDir())) {
    OnBrowsingDataRemoverDone();
    return;
  }
  signin_profile_clear_requested_ = true;
  BrowsingDataRemover* remover =
      BrowsingDataRemover::CreateForUnboundedRange(GetSigninProfile());
  remover->AddObserver(this);
  remover->Remove(BrowsingDataRemover::REMOVE_SITE_DATA,
                  BrowsingDataHelper::ALL);
}

////////////////////////////////////////////////////////////////////////////////
// ProfileHelper, BrowsingDataRemover::Observer implementation:

void ProfileHelper::OnBrowsingDataRemoverDone() {
  signin_profile_clear_requested_ = false;
  for (size_t i = 0; i < on_clear_callbacks_.size(); ++i) {
    if (!on_clear_callbacks_[i].is_null())
      on_clear_callbacks_[i].Run();
  }
  on_clear_callbacks_.clear();
}

////////////////////////////////////////////////////////////////////////////////
// ProfileHelper, OAuth2LoginManager::Observer implementation:

void ProfileHelper::OnSessionRestoreStateChanged(
    Profile* user_profile,
    OAuth2LoginManager::SessionRestoreState state) {
  if (state == OAuth2LoginManager::SESSION_RESTORE_DONE ||
      state == OAuth2LoginManager::SESSION_RESTORE_FAILED ||
      state == OAuth2LoginManager::SESSION_RESTORE_CONNECTION_FAILED) {
    chromeos::OAuth2LoginManager* login_manager =
        chromeos::OAuth2LoginManagerFactory::GetInstance()->
            GetForProfile(user_profile);
    login_manager->RemoveObserver(this);
    ClearSigninProfile(base::Closure());
  }
}

////////////////////////////////////////////////////////////////////////////////
// ProfileHelper, UserManager::UserSessionStateObserver implementation:

void ProfileHelper::ActiveUserHashChanged(const std::string& hash) {
  active_user_id_hash_ = hash;
  base::FilePath profile_path = GetProfilePathByUserIdHash(hash);
  VLOG(1) << "Switching to profile path: " << profile_path.value();
}

}  // namespace chromeos

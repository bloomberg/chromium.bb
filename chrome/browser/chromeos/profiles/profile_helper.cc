// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/profiles/profile_helper.h"

#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/sms_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"

namespace chromeos {

namespace {

base::FilePath GetProfilePathByUserIdHash(const std::string user_id_hash) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  // Fails for KioskTest.InstallAndLaunchApp test - crbug.com/238985
  // Will probably fail for Guest session / restart after a crash -
  // crbug.com/238998
  // TODO(nkostylev): Remove this check once these bugs are fixed.
  if (command_line.HasSwitch(switches::kMultiProfiles))
    DCHECK(!user_id_hash.empty());
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath profile_path = profile_manager->user_data_dir();
  return profile_path.Append(
      base::FilePath(ProfileHelper::kProfileDirPrefix + user_id_hash));
}

} // namespace

////////////////////////////////////////////////////////////////////////////////
// ProfileHelper, public

// static
const char ProfileHelper::kProfileDirPrefix[] = "u-";

ProfileHelper::ProfileHelper()
  : signin_profile_clear_requested_(false) {
}

ProfileHelper::~ProfileHelper() {
}

// static
Profile* ProfileHelper::GetProfileByUserIdHash(
    const std::string& user_id_hash) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  return profile_manager->GetProfile(GetProfilePathByUserIdHash(user_id_hash));
}

// static
Profile* ProfileHelper::GetSigninProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath user_data_dir = profile_manager->user_data_dir();
  base::FilePath signin_profile_dir =
      user_data_dir.AppendASCII(chrome::kInitialProfile);
  return profile_manager->GetProfile(signin_profile_dir)->
      GetOffTheRecordProfile();
}

// static
std::string ProfileHelper::GetUserIdHashFromProfile(Profile* profile) {
  if (!profile)
    return std::string();

  // Check that profile directory starts with the correct prefix.
  std::string profile_dir = profile->GetPath().BaseName().value();
  std::string prefix(ProfileHelper::kProfileDirPrefix);
  if (profile_dir.find(prefix) != 0) {
    NOTREACHED();
    return std::string();
  }

  return profile_dir.substr(prefix.length(),
                            profile_dir.length() - prefix.length());
}

// static
bool ProfileHelper::IsSigninProfile(Profile* profile) {
  return profile->GetPath().BaseName().value() == chrome::kInitialProfile;
}

// static
void ProfileHelper::ProfileStartup(Profile* profile, bool process_startup) {
  // Initialize Chrome OS preferences like touch pad sensitivity. For the
  // preferences to work in the guest mode, the initialization has to be
  // done after |profile| is switched to the incognito profile (which
  // is actually GuestSessionProfile in the guest mode). See the
  // GetOffTheRecordProfile() call above.
  profile->InitChromeOSPreferences();

  if (process_startup) {
    static chromeos::SmsObserver* sms_observer =
        new chromeos::SmsObserver();
    chromeos::CrosLibrary::Get()->GetNetworkLibrary()->
        AddNetworkManagerObserver(sms_observer);

    profile->SetupChromeOSEnterpriseExtensionObserver();
  }
}

base::FilePath ProfileHelper::GetActiveUserProfileDir() {
  DCHECK(!active_user_id_hash_.empty());
  return base::FilePath(
      ProfileHelper::kProfileDirPrefix + active_user_id_hash_);
}

void ProfileHelper::Initialize() {
  UserManager::Get()->AddSessionStateObserver(this);
}

void ProfileHelper::ClearSigninProfile(const base::Closure& on_clear_callback) {
  on_clear_callbacks_.push_back(on_clear_callback);
  if (signin_profile_clear_requested_)
    return;
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
// ProfileHelper, UserManager::UserSessionStateObserver implementation:

void ProfileHelper::ActiveUserHashChanged(const std::string& hash) {
  active_user_id_hash_ = hash;
  base::FilePath profile_path = GetProfilePathByUserIdHash(hash);
  LOG(INFO) << "Switching to profile path: " << profile_path.value();
}

}  // namespace chromeos

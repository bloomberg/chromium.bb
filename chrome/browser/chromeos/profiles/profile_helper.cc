// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/profiles/profile_helper.h"

#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
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

// TODO(nkostylev): Remove this hack when http://crbug.com/224291 is fixed.
// Now user homedirs are mounted to /home/user which is different from
// user data dir (/home/chronos).
base::FilePath GetChromeOSProfileDir(const base::FilePath& path) {
  base::FilePath profile_dir(FILE_PATH_LITERAL("/home/user/"));
  profile_dir = profile_dir.Append(path);
  return profile_dir;
}

} // namespace

////////////////////////////////////////////////////////////////////////////////
// ProfileHelper, public

ProfileHelper::ProfileHelper() {
}

ProfileHelper::~ProfileHelper() {
}

// static
Profile* ProfileHelper::GetProfileByUserIdHash(
    const std::string& user_id_hash) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  return profile_manager->GetProfile(
      GetChromeOSProfileDir(base::FilePath(user_id_hash)));
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

void ProfileHelper::Initialize() {
  UserManager::Get()->AddSessionStateObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// ProfileHelper, UserManager::UserSessionStateObserver implementation:

void ProfileHelper::ActiveUserHashChanged(const std::string& hash) {
  active_user_id_hash_ = hash;
  LOG(INFO) << "Switching to custom profile_dir: " << active_user_id_hash_;
}

}  // namespace chromeos

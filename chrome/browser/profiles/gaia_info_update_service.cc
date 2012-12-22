// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/gaia_info_update_service.h"

#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_details.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"

namespace {

// Update the user's GAIA info every 24 hours.
const int kUpdateIntervalHours = 24;

// If the users's GAIA info is very out of date then wait at least this long
// before starting an update. This avoids slowdown during startup.
const int kMinUpdateIntervalSeconds = 5;

} // namespace

GAIAInfoUpdateService::GAIAInfoUpdateService(Profile* profile)
    : profile_(profile) {
  PrefService* prefs = profile_->GetPrefs();
  username_pref_.Init(prefs::kGoogleServicesUsername, prefs,
                      base::Bind(&GAIAInfoUpdateService::OnUsernameChanged,
                                 base::Unretained(this)));

  last_updated_ = base::Time::FromInternalValue(
      prefs->GetInt64(prefs::kProfileGAIAInfoUpdateTime));
  ScheduleNextUpdate();
}

GAIAInfoUpdateService::~GAIAInfoUpdateService() {
}

void GAIAInfoUpdateService::Update() {
  // The user must be logged in.
  std::string username = profile_->GetPrefs()->GetString(
      prefs::kGoogleServicesUsername);
  if (username.empty())
    return;

  if (profile_image_downloader_.get())
    return;
  profile_image_downloader_.reset(new ProfileDownloader(this));
  profile_image_downloader_->Start();
}

// static
bool GAIAInfoUpdateService::ShouldUseGAIAProfileInfo(Profile* profile) {
#if defined(OS_CHROMEOS)
  return false;
#endif

  // Sync must be allowed.
  if (!profile->GetOriginalProfile()->IsSyncAccessible())
    return false;

  // To enable this feature for testing pass "--gaia-profile-info".
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kGaiaProfileInfo)) {
    return true;
  }

  // This feature is disable by default.
  return false;
}

// static
void GAIAInfoUpdateService::RegisterUserPrefs(PrefServiceSyncable* prefs) {
  prefs->RegisterInt64Pref(prefs::kProfileGAIAInfoUpdateTime,
                           0,
                           PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kProfileGAIAInfoPictureURL,
                            "",
                            PrefServiceSyncable::UNSYNCABLE_PREF);
}

bool GAIAInfoUpdateService::NeedsProfilePicture() const {
  return true;
}

int GAIAInfoUpdateService::GetDesiredImageSideLength() const {
  return 256;
}

Profile* GAIAInfoUpdateService::GetBrowserProfile() {
  return profile_;
}

std::string GAIAInfoUpdateService::GetCachedPictureURL() const {
  return profile_->GetPrefs()->GetString(prefs::kProfileGAIAInfoPictureURL);
}

void GAIAInfoUpdateService::OnProfileDownloadSuccess(
    ProfileDownloader* downloader) {
  // Make sure that |ProfileDownloader| gets deleted after return.
  scoped_ptr<ProfileDownloader> profile_image_downloader(
      profile_image_downloader_.release());

  // Save the last updated time.
  last_updated_ = base::Time::Now();
  profile_->GetPrefs()->SetInt64(prefs::kProfileGAIAInfoUpdateTime,
                                 last_updated_.ToInternalValue());
  ScheduleNextUpdate();

  string16 full_name = downloader->GetProfileFullName();
  SkBitmap bitmap = downloader->GetProfilePicture();
  ProfileDownloader::PictureStatus picture_status =
      downloader->GetProfilePictureStatus();
  std::string picture_url = downloader->GetProfilePictureURL();

  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  size_t profile_index = cache.GetIndexOfProfileWithPath(profile_->GetPath());
  if (profile_index == std::string::npos)
    return;

  cache.SetGAIANameOfProfileAtIndex(profile_index, full_name);
  // The profile index may have changed.
  profile_index = cache.GetIndexOfProfileWithPath(profile_->GetPath());
  if (profile_index == std::string::npos)
    return;
  if (picture_status == ProfileDownloader::PICTURE_SUCCESS) {
    profile_->GetPrefs()->SetString(prefs::kProfileGAIAInfoPictureURL,
                                    picture_url);
    gfx::Image gfx_image(bitmap);
    cache.SetGAIAPictureOfProfileAtIndex(profile_index, &gfx_image);
  } else if (picture_status == ProfileDownloader::PICTURE_DEFAULT) {
    cache.SetGAIAPictureOfProfileAtIndex(profile_index, NULL);
  }

  // If this profile hasn't switched to using GAIA information for the profile
  // name and picture then switch it now. Once the profile has switched this
  // preference guards against clobbering the user's custom settings.
  if (!cache.GetHasMigratedToGAIAInfoOfProfileAtIndex(profile_index)) {
    cache.SetHasMigratedToGAIAInfoOfProfileAtIndex(profile_index, true);
    // Order matters here for shortcut management, like in
    // ProfileShortcutManagerWin::OnProfileAdded, as the picture update does not
    // allow us to change the target, so we have to apply any renaming first. We
    // also need to re-fetch the index, as SetIsUsingGAIANameOfProfileAtIndex
    // may alter it.
    cache.SetIsUsingGAIANameOfProfileAtIndex(profile_index, true);
    profile_index = cache.GetIndexOfProfileWithPath(profile_->GetPath());
    cache.SetIsUsingGAIAPictureOfProfileAtIndex(profile_index, true);
  }
}

void GAIAInfoUpdateService::OnProfileDownloadFailure(
    ProfileDownloader* downloader,
    ProfileDownloaderDelegate::FailureReason reason) {
  profile_image_downloader_.reset();

  // Save the last updated time.
  last_updated_ = base::Time::Now();
  profile_->GetPrefs()->SetInt64(prefs::kProfileGAIAInfoUpdateTime,
                                 last_updated_.ToInternalValue());
  ScheduleNextUpdate();
}

void GAIAInfoUpdateService::OnUsernameChanged() {
  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  size_t profile_index = cache.GetIndexOfProfileWithPath(profile_->GetPath());
  if (profile_index == std::string::npos)
    return;

  std::string username = profile_->GetPrefs()->GetString(
      prefs::kGoogleServicesUsername);
  if (username.empty()) {
    // Unset the old user's GAIA info.
    cache.SetGAIANameOfProfileAtIndex(profile_index, string16());
    // The profile index may have changed.
    profile_index = cache.GetIndexOfProfileWithPath(profile_->GetPath());
    if (profile_index == std::string::npos)
      return;
    cache.SetGAIAPictureOfProfileAtIndex(profile_index, NULL);
    // Unset the cached URL.
    profile_->GetPrefs()->ClearPref(prefs::kProfileGAIAInfoPictureURL);
  } else {
    // Update the new user's GAIA info.
    Update();
  }
}

void GAIAInfoUpdateService::ScheduleNextUpdate() {
  if (timer_.IsRunning())
    return;

  const base::TimeDelta desired_delta =
      base::TimeDelta::FromHours(kUpdateIntervalHours);
  const base::TimeDelta update_delta = base::Time::Now() - last_updated_;

  base::TimeDelta delta;
  if (update_delta < base::TimeDelta() || update_delta > desired_delta)
    delta = base::TimeDelta::FromSeconds(kMinUpdateIntervalSeconds);
  else
    delta = desired_delta - update_delta;

  timer_.Start(FROM_HERE, delta, this, &GAIAInfoUpdateService::Update);
}

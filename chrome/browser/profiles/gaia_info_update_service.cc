// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/gaia_info_update_service.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/common/chrome_notification_types.h"
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
  username_pref_.Init(prefs::kGoogleServicesUsername, prefs, this);

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

  return true;
}

// static
void GAIAInfoUpdateService::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterInt64Pref(
      prefs::kProfileGAIAInfoUpdateTime, 0, PrefService::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(
      prefs::kProfileGAIAInfoPictureURL, "", PrefService::UNSYNCABLE_PREF);
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

void GAIAInfoUpdateService::OnDownloadComplete(ProfileDownloader* downloader,
                                               bool success) {
  // Save the last updated time.
  last_updated_ = base::Time::Now();
  profile_->GetPrefs()->SetInt64(prefs::kProfileGAIAInfoUpdateTime,
                                 last_updated_.ToInternalValue());
  ScheduleNextUpdate();

  if (!success) {
    profile_image_downloader_.reset();
    return;
  }

  string16 full_name = downloader->GetProfileFullName();
  SkBitmap bitmap = downloader->GetProfilePicture();
  ProfileDownloader::PictureStatus picture_status =
      downloader->GetProfilePictureStatus();
  std::string picture_url = downloader->GetProfilePictureURL();
  profile_image_downloader_.reset();

  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  size_t profile_index = cache.GetIndexOfProfileWithPath(profile_->GetPath());
  if (profile_index == std::string::npos)
    return;

  cache.SetGAIANameOfProfileAtIndex(profile_index, full_name);
  if (picture_status == ProfileDownloader::PICTURE_SUCCESS) {
    profile_->GetPrefs()->SetString(prefs::kProfileGAIAInfoPictureURL,
                                    picture_url);
    gfx::Image gfx_image(new SkBitmap(bitmap));
    cache.SetGAIAPictureOfProfileAtIndex(profile_index, &gfx_image);
  } else if (picture_status == ProfileDownloader::PICTURE_DEFAULT) {
    cache.SetGAIAPictureOfProfileAtIndex(profile_index, NULL);
  }

  // If this profile hasn't switched to using GAIA information for the profile
  // name and picture then switch it now. Once the profile has switched this
  // preference guards against clobbering the user's custom settings.
  if (!cache.GetHasMigratedToGAIAInfoOfProfileAtIndex(profile_index)) {
    cache.SetHasMigratedToGAIAInfoOfProfileAtIndex(profile_index, true);
    cache.SetIsUsingGAIAPictureOfProfileAtIndex(profile_index, true);
    cache.SetIsUsingGAIANameOfProfileAtIndex(profile_index, true);
  }
}

void GAIAInfoUpdateService::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_PREF_CHANGED) {
    std::string* name = content::Details<std::string>(details).ptr();
    if (prefs::kGoogleServicesUsername == *name)
      OnUsernameChanged();
  } else {
    NOTREACHED();
  }
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
    cache.SetGAIAPictureOfProfileAtIndex(profile_index, NULL);
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

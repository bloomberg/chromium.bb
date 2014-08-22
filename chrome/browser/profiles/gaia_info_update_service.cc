// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/gaia_info_update_service.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/common/pref_names.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "content/public/browser/notification_details.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"

namespace {

// Update the user's GAIA info every 24 hours.
const int kUpdateIntervalHours = 24;

// If the users's GAIA info is very out of date then wait at least this long
// before starting an update. This avoids slowdown during startup.
const int kMinUpdateIntervalSeconds = 5;

}  // namespace

GAIAInfoUpdateService::GAIAInfoUpdateService(Profile* profile)
    : profile_(profile) {
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile_);
  signin_manager->AddObserver(this);

  PrefService* prefs = profile_->GetPrefs();
  last_updated_ = base::Time::FromInternalValue(
      prefs->GetInt64(prefs::kProfileGAIAInfoUpdateTime));
  ScheduleNextUpdate();
}

GAIAInfoUpdateService::~GAIAInfoUpdateService() {
  DCHECK(!profile_) << "Shutdown not called before dtor";
}

void GAIAInfoUpdateService::Update() {
  // The user must be logged in.
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile_);
  if (signin_manager->GetAuthenticatedAccountId().empty())
    return;

  if (profile_image_downloader_)
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

  // To enable this feature for testing pass "--google-profile-info".
  if (switches::IsGoogleProfileInfo())
    return true;

  // This feature is disable by default.
  return false;
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

  base::string16 full_name = downloader->GetProfileFullName();
  base::string16 given_name = downloader->GetProfileGivenName();
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
  DCHECK_NE(profile_index, std::string::npos);

  cache.SetGAIAGivenNameOfProfileAtIndex(profile_index, given_name);
  // The profile index may have changed.
  profile_index = cache.GetIndexOfProfileWithPath(profile_->GetPath());
  DCHECK_NE(profile_index, std::string::npos);

  if (picture_status == ProfileDownloader::PICTURE_SUCCESS) {
    profile_->GetPrefs()->SetString(prefs::kProfileGAIAInfoPictureURL,
                                    picture_url);
    gfx::Image gfx_image = gfx::Image::CreateFrom1xBitmap(bitmap);
    cache.SetGAIAPictureOfProfileAtIndex(profile_index, &gfx_image);
  } else if (picture_status == ProfileDownloader::PICTURE_DEFAULT) {
    cache.SetGAIAPictureOfProfileAtIndex(profile_index, NULL);
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

void GAIAInfoUpdateService::OnUsernameChanged(const std::string& username) {
  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  size_t profile_index = cache.GetIndexOfProfileWithPath(profile_->GetPath());
  if (profile_index == std::string::npos)
    return;

  if (username.empty()) {
    // Unset the old user's GAIA info.
    cache.SetGAIANameOfProfileAtIndex(profile_index, base::string16());
    cache.SetGAIAGivenNameOfProfileAtIndex(profile_index, base::string16());
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

void GAIAInfoUpdateService::Shutdown() {
  timer_.Stop();
  profile_image_downloader_.reset();
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile_);
  signin_manager->RemoveObserver(this);

  // OK to reset |profile_| pointer here because GAIAInfoUpdateService will not
  // access it again.  This pointer is also used to implement the delegate for
  // |profile_image_downloader_|.  However that object was destroyed above.
  profile_ = NULL;
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

void GAIAInfoUpdateService::GoogleSigninSucceeded(
    const std::string& username,
    const std::string& password) {
  OnUsernameChanged(username);
}

void GAIAInfoUpdateService::GoogleSignedOut(const std::string& username) {
  OnUsernameChanged(std::string());
}

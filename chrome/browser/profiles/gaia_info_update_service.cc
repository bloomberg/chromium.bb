// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"

GAIAInfoUpdateService::GAIAInfoUpdateService(Profile* profile)
    : profile_(profile) {
}

GAIAInfoUpdateService::~GAIAInfoUpdateService() {
}

void GAIAInfoUpdateService::Update() {
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

  // The user must be logged in.
  ProfileSyncService* service =
      profile->GetOriginalProfile()->GetProfileSyncService();
  if (!service || !service->HasSyncSetupCompleted())
    return false;

  // TODO(sail): For now put this feature behind a flag.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kGaiaProfileInfo)) {
    return false;
  }

  return true;
}

int GAIAInfoUpdateService::GetDesiredImageSideLength() {
  return 256;
}

Profile* GAIAInfoUpdateService::GetBrowserProfile() {
  return profile_;
}

void GAIAInfoUpdateService::OnDownloadComplete(ProfileDownloader* downloader,
                                               bool success) {
  if (!success) {
    profile_image_downloader_.reset();
    return;
  }

  string16 full_name = downloader->GetProfileFullName();
  SkBitmap bitmap = downloader->GetProfilePicture();
  profile_image_downloader_.reset();

  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  size_t profile_index = cache.GetIndexOfProfileWithPath(profile_->GetPath());
  if (profile_index == std::string::npos)
    return;

  cache.SetGAIANameOfProfileAtIndex(profile_index, full_name);
  gfx::Image gfx_image(new SkBitmap(bitmap));
  cache.SetGAIAPictureOfProfileAtIndex(profile_index, gfx_image);

  // If this profile hasn't switched to using GAIA information for the profile
  // name and picture then switch it now. Once the profile has switched this
  // preference guards against clobbering the user's custom settings.
  if (!cache.GetHasMigratedToGAIAInfoOfProfileAtIndex(profile_index)) {
    cache.SetHasMigratedToGAIAInfoOfProfileAtIndex(profile_index, true);
    cache.SetIsUsingGAIAPictureOfProfileAtIndex(profile_index, true);
    cache.SetIsUsingGAIANameOfProfileAtIndex(profile_index, true);
  }
}

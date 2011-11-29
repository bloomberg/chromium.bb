// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_GAIA_INFO_UPDATE_SERVICE_H_
#define CHROME_BROWSER_PROFILES_GAIA_INFO_UPDATE_SERVICE_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/profiles/profile_downloader_delegate.h"
#include "chrome/browser/profiles/profile_downloader.h"

class Profile;
class ProfileDownloader;

// This service kicks off a download of the user's name and profile picture.
// The results are saved in the profile info cache.
class GAIAInfoUpdateService : public ProfileDownloaderDelegate {
 public:
  explicit GAIAInfoUpdateService(Profile* profile);
  virtual ~GAIAInfoUpdateService();

  // Updates the GAIA info for the profile associated with this instance.
  void Update();

  // Checks if downloading GAIA info for the given profile is allowed.
  static bool ShouldUseGAIAProfileInfo(Profile* profile);

  // ProfileDownloaderDelegate:
  virtual int GetDesiredImageSideLength() OVERRIDE;
  virtual Profile* GetBrowserProfile() OVERRIDE;
  virtual void OnDownloadComplete(ProfileDownloader* downloader,
                                  bool success) OVERRIDE;

 private:
  Profile* profile_;
  scoped_ptr<ProfileDownloader> profile_image_downloader_;

  DISALLOW_COPY_AND_ASSIGN(GAIAInfoUpdateService);
};

#endif  // CHROME_BROWSER_PROFILES_GAIA_INFO_UPDATE_SERVICE_H_

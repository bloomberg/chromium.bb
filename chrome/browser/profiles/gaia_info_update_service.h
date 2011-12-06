// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_GAIA_INFO_UPDATE_SERVICE_H_
#define CHROME_BROWSER_PROFILES_GAIA_INFO_UPDATE_SERVICE_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/profiles/profile_downloader_delegate.h"
#include "chrome/browser/profiles/profile_downloader.h"
#include "content/public/browser/notification_observer.h"

class Profile;
class ProfileDownloader;

// This service kicks off a download of the user's name and profile picture.
// The results are saved in the profile info cache.
class GAIAInfoUpdateService : public ProfileDownloaderDelegate,
                              public content::NotificationObserver {
 public:
  explicit GAIAInfoUpdateService(Profile* profile);
  virtual ~GAIAInfoUpdateService();

  // Updates the GAIA info for the profile associated with this instance.
  virtual void Update();

  // Checks if downloading GAIA info for the given profile is allowed.
  static bool ShouldUseGAIAProfileInfo(Profile* profile);

  // Register prefs for a profile.
  static void RegisterUserPrefs(PrefService* prefs);

  // ProfileDownloaderDelegate:
  virtual int GetDesiredImageSideLength() const OVERRIDE;
  virtual Profile* GetBrowserProfile() OVERRIDE;
  virtual std::string GetCachedPictureURL() const OVERRIDE;
  virtual void OnDownloadComplete(ProfileDownloader* downloader,
                                  bool success) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(GAIAInfoUpdateServiceTest, ScheduleUpdate);

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;


  void OnUsernameChanged();
  void ScheduleNextUpdate();

  Profile* profile_;
  scoped_ptr<ProfileDownloader> profile_image_downloader_;
  StringPrefMember username_pref_;
  base::Time last_updated_;
  base::OneShotTimer<GAIAInfoUpdateService> timer_;

  DISALLOW_COPY_AND_ASSIGN(GAIAInfoUpdateService);
};

#endif  // CHROME_BROWSER_PROFILES_GAIA_INFO_UPDATE_SERVICE_H_

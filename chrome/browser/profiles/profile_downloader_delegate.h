// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_DOWNLOADER_DELEGATE_H_
#define CHROME_BROWSER_PROFILES_PROFILE_DOWNLOADER_DELEGATE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/string16.h"

class Profile;
class ProfileDownloader;

// Reports on success or failure of Profile download. It is OK to delete the
// |ProfileImageDownloader| instance in any of these handlers.
class ProfileDownloaderDelegate {
 public:
  virtual ~ProfileDownloaderDelegate() {}

  // Returns the desired side length of the profile image. If 0, returns image
  // of the originally uploaded size.
  virtual int GetDesiredImageSideLength() = 0;

  // Returns the browser profile associated with this download request.
  virtual Profile* GetBrowserProfile() = 0;

  // Whether we should use an OAuth refresh token or the Picasa token from
  // ClientLogin. The latter is currently used in ChromeOS.
  virtual bool ShouldUseOAuthRefreshToken() = 0;

  // Called when the download is complete. On success delegate should query
  // the downloader for values.
  virtual void OnDownloadComplete(ProfileDownloader* downloader,
                                  bool success) = 0;
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_DOWNLOADER_DELEGATE_H_

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
  virtual int GetDesiredImageSideLength() const = 0;

  // Returns the cached URL. If the cache URL matches the new image URL
  // the image will not be downloaded. Return an empty string when there is no
  // cached URL.
  virtual std::string GetCachedPictureURL() const = 0;

  // Returns the browser profile associated with this download request.
  virtual Profile* GetBrowserProfile() = 0;

  // Called when the download is complete. On success delegate should query
  // the downloader for values.
  virtual void OnDownloadComplete(ProfileDownloader* downloader,
                                  bool success) = 0;
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_DOWNLOADER_DELEGATE_H_

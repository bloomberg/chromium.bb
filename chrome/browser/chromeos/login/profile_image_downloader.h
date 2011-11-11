// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_PROFILE_IMAGE_DOWNLOADER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_PROFILE_IMAGE_DOWNLOADER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/login/image_decoder.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/common/url_fetcher_delegate.h"
#include "googleurl/src/gurl.h"

namespace chromeos {

// Downloads user profile image, decodes it in a sandboxed process.
class ProfileImageDownloader : public content::URLFetcherDelegate,
                               public ImageDecoder::Delegate,
                               public content::NotificationObserver {
 public:
  // Enum for reporting histograms about profile picture download.
  enum DownloadResult {
    kDownloadSuccessChanged,
    kDownloadSuccess,
    kDownloadFailure,
    kDownloadDefault,

    // Must be the last, convenient count.
    kDownloadResultsCount
  };

  // Reports on success or failure of Profile image download. It is OK to
  // delete the |ProfileImageDownloader| instance in any of these handlers.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called when image is successfully downloaded and decoded.
    virtual void OnDownloadSuccess(const SkBitmap& image) = 0;

    // Called on download failure.
    virtual void OnDownloadFailure() {}

    // Called when user has the default profile image and we won't download
    // it.
    virtual void OnDownloadDefaultImage() {}
  };

  explicit ProfileImageDownloader(Delegate* delegate);
  virtual ~ProfileImageDownloader();

  // Starts downloading the picture if the necessary authorization token is
  // ready. If not, subscribes to token service and starts fetching if the
  // token is available. Should not be called more than once.
  void Start();

 private:
  // Overriden from content::URLFetcherDelegate:
  virtual void OnURLFetchComplete(const content::URLFetcher* source) OVERRIDE;

  // Overriden from ImageDecoder::Delegate:
  virtual void OnImageDecoded(const ImageDecoder* decoder,
                              const SkBitmap& decoded_image) OVERRIDE;
  virtual void OnDecodeImageFailed(const ImageDecoder* decoder) OVERRIDE;

  // Overriden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Searches for profile image URL in user entry response from Picasa.
  // Returns an empty string on failure.
  std::string GetProfileImageURL(const std::string& data) const;

  // Returns true if the image url is url of the default profile picture.
  bool IsDefaultProfileImageURL(const std::string& url) const;

  // Issues the first request to get user profile image.
  void StartFetchingImage();

  Delegate* delegate_;
  std::string auth_token_;
  scoped_ptr<content::URLFetcher> user_entry_fetcher_;
  scoped_ptr<content::URLFetcher> profile_image_fetcher_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ProfileImageDownloader);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_PROFILE_IMAGE_DOWNLOADER_H_

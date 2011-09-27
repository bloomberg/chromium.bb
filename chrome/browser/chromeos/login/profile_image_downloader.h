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
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "content/common/net/url_fetcher.h"
#include "googleurl/src/gurl.h"

namespace chromeos {

// Downloads user profile image, decodes it in a sandboxed process.
class ProfileImageDownloader : public URLFetcher::Delegate,
                               public ImageDecoder::Delegate,
                               public NotificationObserver {
 public:
  // Reports on success or failure of Profile image download.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called when image is successfully downloaded and decoded.
    virtual void OnDownloadSuccess(const SkBitmap& image) = 0;

    // Called on download failure.
    virtual void OnDownloadFailure() {}
  };

  explicit ProfileImageDownloader(Delegate* delegate);
  virtual ~ProfileImageDownloader();

  // Starts downloading the picture if the necessary authorization token is
  // ready. If not, subscribes to token service and starts fetching if the
  // token is available.
  void Start();

 private:
  // Overriden from URLFetcher::Delegate:
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const net::URLRequestStatus& status,
                                  int response_code,
                                  const net::ResponseCookies& cookies,
                                  const std::string& data) OVERRIDE;

  // Overriden from ImageDecoder::Delegate:
  virtual void OnImageDecoded(const ImageDecoder* decoder,
                              const SkBitmap& decoded_image) OVERRIDE;
  virtual void OnDecodeImageFailed(const ImageDecoder* decoder) OVERRIDE;

  // Overriden from NotificationObserver:
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // Searches for profile image URL in user entry response from Picasa.
  // Returns an empty string on failure.
  std::string GetProfileImageURL(const std::string& data) const;

  // Issues the first request to get user profile image.
  void StartFetchingImage();

  Delegate* delegate_;
  std::string auth_token_;
  scoped_ptr<URLFetcher> user_entry_fetcher_;
  scoped_ptr<URLFetcher> profile_image_fetcher_;
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ProfileImageDownloader);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_PROFILE_IMAGE_DOWNLOADER_H_


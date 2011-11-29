// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_DOWNLOADER_H_
#define CHROME_BROWSER_PROFILES_PROFILE_DOWNLOADER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/image_decoder.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/common/url_fetcher_delegate.h"
#include "googleurl/src/gurl.h"
#include "third_party/skia/include/core/SkBitmap.h"

class ProfileDownloaderDelegate;

// Downloads user profile information. The profile picture is decoded in a
// sandboxed process.
class ProfileDownloader : public content::URLFetcherDelegate,
                          public ImageDecoder::Delegate,
                          public content::NotificationObserver {
 public:
  explicit ProfileDownloader(ProfileDownloaderDelegate* delegate);
  virtual ~ProfileDownloader();

  // Starts downloading profile information if the necessary authorization token
  // is ready. If not, subscribes to token service and starts fetching if the
  // token is available. Should not be called more than once.
  void Start();

  // On successful download this returns the full name of the user. For example
  // "Pat Smith".
  const string16& GetProfileFullName() const;

  // On successful download this returns the profile picture of the user.
  // For users with no profile picture set (that is, they have the default
  // profile picture) this will return an Null bitmap.
  const SkBitmap& GetProfilePicture() const;

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

  // Parses the entry response from Picasa and gets the nick name and
  // and profile image URL. Returns false to indicate a parsing error.
  bool GetProfileNickNameAndImageURL(const std::string& data,
                                     string16* nick_name,
                                     std::string* url) const;

  // Returns true if the image url is url of the default profile picture.
  bool IsDefaultProfileImageURL(const std::string& url) const;

  // Issues the first request to get user profile image.
  void StartFetchingImage();

  ProfileDownloaderDelegate* delegate_;
  std::string auth_token_;
  scoped_ptr<content::URLFetcher> user_entry_fetcher_;
  scoped_ptr<content::URLFetcher> profile_image_fetcher_;
  content::NotificationRegistrar registrar_;
  string16 profile_full_name_;
  SkBitmap profile_picture_;

  DISALLOW_COPY_AND_ASSIGN(ProfileDownloader);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_DOWNLOADER_H_

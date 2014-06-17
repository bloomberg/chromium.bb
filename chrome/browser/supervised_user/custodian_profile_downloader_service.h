// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_CUSTODIAN_PROFILE_DOWNLOADER_SERVICE_H_
#define CHROME_BROWSER_SUPERVISED_USER_CUSTODIAN_PROFILE_DOWNLOADER_SERVICE_H_

#include "base/callback.h"
#include "chrome/browser/profiles/profile_downloader.h"
#include "chrome/browser/profiles/profile_downloader_delegate.h"
#include "components/keyed_service/core/keyed_service.h"

class CustodianProfileDownloaderService : public KeyedService,
                                          public ProfileDownloaderDelegate {
 public:
  // Callback for DownloadProfile() below. If the GAIA profile download is
  // successful, the profile's full (display) name will be returned.
  typedef base::Callback<void(const base::string16& /* full name */)>
      DownloadProfileCallback;

  virtual ~CustodianProfileDownloaderService();

  // KeyedService:
  virtual void Shutdown() OVERRIDE;

  // Downloads the GAIA account information for the |custodian_profile_|.
  // This is a best-effort attempt with no error reporting nor timeout.
  // If the download is successful, the profile's full (display) name will
  // be returned via the callback. If the download fails or never completes,
  // the callback will not be called.
  void DownloadProfile(const DownloadProfileCallback& callback);

  // ProfileDownloaderDelegate:
  virtual bool NeedsProfilePicture() const OVERRIDE;
  virtual int GetDesiredImageSideLength() const OVERRIDE;
  virtual std::string GetCachedPictureURL() const OVERRIDE;
  virtual Profile* GetBrowserProfile() OVERRIDE;
  virtual void OnProfileDownloadSuccess(ProfileDownloader* downloader) OVERRIDE;
  virtual void OnProfileDownloadFailure(
      ProfileDownloader* downloader,
      ProfileDownloaderDelegate::FailureReason reason) OVERRIDE;

 private:
  friend class CustodianProfileDownloaderServiceFactory;
  // Use |CustodianProfileDownloaderServiceFactory::GetForProfile(...)| to
  // get instances of this service.
  explicit CustodianProfileDownloaderService(Profile* custodian_profile);

  scoped_ptr<ProfileDownloader> profile_downloader_;
  DownloadProfileCallback download_callback_;

  // Owns us via the KeyedService mechanism.
  Profile* custodian_profile_;

  std::string last_downloaded_profile_email_;
  std::string in_progress_profile_email_;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_CUSTODIAN_PROFILE_DOWNLOADER_SERVICE_H_

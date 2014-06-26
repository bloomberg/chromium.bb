// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_DOWNLOADER_H_
#define CHROME_BROWSER_PROFILES_PROFILE_DOWNLOADER_H_

#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/image_decoder.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

class ProfileDownloaderDelegate;
class OAuth2AccessTokenFetcher;

namespace net {
class URLFetcher;
}  // namespace net

// Downloads user profile information. The profile picture is decoded in a
// sandboxed process.
class ProfileDownloader : public net::URLFetcherDelegate,
                          public ImageDecoder::Delegate,
                          public OAuth2TokenService::Observer,
                          public OAuth2TokenService::Consumer {
 public:
  enum PictureStatus {
    PICTURE_SUCCESS,
    PICTURE_FAILED,
    PICTURE_DEFAULT,
    PICTURE_CACHED,
  };

  explicit ProfileDownloader(ProfileDownloaderDelegate* delegate);
  virtual ~ProfileDownloader();

  // Starts downloading profile information if the necessary authorization token
  // is ready. If not, subscribes to token service and starts fetching if the
  // token is available. Should not be called more than once.
  virtual void Start();

  // Starts downloading profile information if the necessary authorization token
  // is ready. If not, subscribes to token service and starts fetching if the
  // token is available. Should not be called more than once.
  virtual void StartForAccount(const std::string& account_id);

  // On successful download this returns the full name of the user. For example
  // "Pat Smith".
  virtual base::string16 GetProfileFullName() const;

  // On successful download this returns the given name of the user. For example
  // if the name is "Pat Smith", the given name is "Pat".
  virtual base::string16 GetProfileGivenName() const;

  // On successful download this returns G+ locale preference of the user.
  virtual std::string GetProfileLocale() const;

  // On successful download this returns the profile picture of the user.
  // For users with no profile picture set (that is, they have the default
  // profile picture) this will return an Null bitmap.
  virtual SkBitmap GetProfilePicture() const;

  // Gets the profile picture status.
  virtual PictureStatus GetProfilePictureStatus() const;

  // Gets the URL for the profile picture. This can be cached so that the same
  // picture is not downloaded multiple times. This value should only be used
  // when the picture status is PICTURE_SUCCESS.
  virtual std::string GetProfilePictureURL() const;

 private:
  friend class ProfileDownloaderTest;
  FRIEND_TEST_ALL_PREFIXES(ProfileDownloaderTest, ParseData);
  FRIEND_TEST_ALL_PREFIXES(ProfileDownloaderTest, DefaultURL);

  // Overriden from net::URLFetcherDelegate:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // Overriden from ImageDecoder::Delegate:
  virtual void OnImageDecoded(const ImageDecoder* decoder,
                              const SkBitmap& decoded_image) OVERRIDE;
  virtual void OnDecodeImageFailed(const ImageDecoder* decoder) OVERRIDE;

  // Overriden from OAuth2TokenService::Observer:
  virtual void OnRefreshTokenAvailable(const std::string& account_id) OVERRIDE;

  // Overriden from OAuth2TokenService::Consumer:
  virtual void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                                 const std::string& access_token,
                                 const base::Time& expiration_time) OVERRIDE;
  virtual void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                                 const GoogleServiceAuthError& error) OVERRIDE;

  // Parses the entry response and gets the name, profile image URL and locale.
  // |data| should be the JSON formatted data return by the response.
  // Returns false to indicate a parsing error.
  static bool ParseProfileJSON(const std::string& data,
                               base::string16* full_name,
                               base::string16* given_name,
                               std::string* url,
                               int image_size,
                               std::string* profile_locale);
  // Returns true if the image url is url of the default profile picture.
  static bool IsDefaultProfileImageURL(const std::string& url);

  // Issues the first request to get user profile image.
  void StartFetchingImage();

  // Gets the authorization header.
  const char* GetAuthorizationHeader() const;

  // Starts fetching OAuth2 access token. This is needed before the GAIA info
  // can be downloaded.
  void StartFetchingOAuth2AccessToken();

  ProfileDownloaderDelegate* delegate_;
  std::string account_id_;
  std::string auth_token_;
  scoped_ptr<net::URLFetcher> user_entry_fetcher_;
  scoped_ptr<net::URLFetcher> profile_image_fetcher_;
  scoped_ptr<OAuth2TokenService::Request> oauth2_access_token_request_;
  base::string16 profile_full_name_;
  base::string16 profile_given_name_;
  std::string profile_locale_;
  SkBitmap profile_picture_;
  PictureStatus picture_status_;
  std::string picture_url_;

  DISALLOW_COPY_AND_ASSIGN(ProfileDownloader);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_DOWNLOADER_H_

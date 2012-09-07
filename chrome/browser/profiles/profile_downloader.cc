// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_downloader.h"

#include <string>
#include <vector>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_downloader_delegate.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "googleurl/src/gurl.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"
#include "skia/ext/image_operations.h"

using content::BrowserThread;

namespace {

// Template for optional authorization header when using an OAuth access token.
const char kAuthorizationHeader[] =
    "Authorization: Bearer %s";

// URL requesting user info.
const char kUserEntryURL[] =
    "https://www.googleapis.com/oauth2/v1/userinfo?alt=json";

// OAuth scope for the user info API.
const char kAPIScope[] = "https://www.googleapis.com/auth/userinfo.profile";

// Path in JSON dictionary to user's photo thumbnail URL.
const char kPhotoThumbnailURLPath[] = "picture";

const char kNickNamePath[] = "name";

// Path format for specifying thumbnail's size.
const char kThumbnailSizeFormat[] = "s%d-c";
// Default thumbnail size.
const int kDefaultThumbnailSize = 64;

// Separator of URL path components.
const char kURLPathSeparator = '/';

// Photo ID of the Picasa Web Albums profile picture (base64 of 0).
const char kPicasaPhotoId[] = "AAAAAAAAAAA";

// Photo version of the default PWA profile picture (base64 of 1).
const char kDefaultPicasaPhotoVersion[] = "AAAAAAAAAAE";

// The minimum number of path components in profile picture URL.
const size_t kProfileImageURLPathComponentsCount = 6;

// Index of path component with photo ID.
const int kPhotoIdPathComponentIndex = 2;

// Index of path component with photo version.
const int kPhotoVersionPathComponentIndex = 3;

// Given an image URL this function builds a new URL set to |size|.
// For example, if |size| was set to 256 and |old_url| was either:
//   https://example.com/--Abc/AAAAAAAAAAI/AAAAAAAAACQ/Efg/photo.jpg
//   or
//   https://example.com/--Abc/AAAAAAAAAAI/AAAAAAAAACQ/Efg/s64-c/photo.jpg
// then return value in |new_url| would be:
//   https://example.com/--Abc/AAAAAAAAAAI/AAAAAAAAACQ/Efg/s256-c/photo.jpg
bool GetImageURLWithSize(const GURL& old_url, int size, GURL* new_url) {
  DCHECK(new_url);
  std::vector<std::string> components;
  base::SplitString(old_url.path(), kURLPathSeparator, &components);
  if (components.size() == 0)
    return false;

  const std::string& old_spec = old_url.spec();
  std::string default_size_component(
      base::StringPrintf(kThumbnailSizeFormat, kDefaultThumbnailSize));
  std::string new_size_component(
      base::StringPrintf(kThumbnailSizeFormat, size));

  size_t pos = old_spec.find(default_size_component);
  size_t end = std::string::npos;
  if (pos != std::string::npos) {
    // The default size is already specified in the URL so it needs to be
    // replaced with the new size.
    end = pos + default_size_component.size();
  } else {
    // The default size is not in the URL so try to insert it before the last
    // component.
    const std::string& file_name = old_url.ExtractFileName();
    if (!file_name.empty()) {
      pos = old_spec.find(file_name);
      end = pos - 1;
    }
  }

  if (pos != std::string::npos) {
    std::string new_spec = old_spec.substr(0, pos) + new_size_component +
                           old_spec.substr(end);
    *new_url = GURL(new_spec);
    return new_url->is_valid();
  }

  // We can't set the image size, just use the default size.
  *new_url = old_url;
  return true;
}

}  // namespace

// static
bool ProfileDownloader::GetProfileNameAndImageURL(const std::string& data,
                                                  string16* nick_name,
                                                  std::string* url,
                                                  int image_size) {
  DCHECK(nick_name);
  DCHECK(url);
  *nick_name = string16();
  *url = std::string();

  int error_code = -1;
  std::string error_message;
  scoped_ptr<base::Value> root_value(base::JSONReader::ReadAndReturnError(
      data, base::JSON_PARSE_RFC, &error_code, &error_message));
  if (!root_value.get()) {
    LOG(ERROR) << "Error while parsing user entry response: "
               << error_message;
    return false;
  }
  if (!root_value->IsType(base::Value::TYPE_DICTIONARY)) {
    LOG(ERROR) << "JSON root is not a dictionary: "
               << root_value->GetType();
    return false;
  }
  base::DictionaryValue* root_dictionary =
      static_cast<base::DictionaryValue*>(root_value.get());

  root_dictionary->GetString(kNickNamePath, nick_name);

  std::string url_string;
  if (root_dictionary->GetString(kPhotoThumbnailURLPath, &url_string)) {
    GURL new_url;
    if (!GetImageURLWithSize(GURL(url_string), image_size, &new_url)) {
      LOG(ERROR) << "GetImageURLWithSize failed for url: " << url_string;
      return false;
    }
    *url = new_url.spec();
  }

  // The profile data is considered valid as long as it has a name or a picture.
  return !nick_name->empty() || !url->empty();
}

// static
bool ProfileDownloader::IsDefaultProfileImageURL(const std::string& url) {
  if (url.empty())
    return true;

  GURL image_url_object(url);
  DCHECK(image_url_object.is_valid());
  VLOG(1) << "URL to check for default image: " << image_url_object.spec();
  std::vector<std::string> path_components;
  base::SplitString(image_url_object.path(),
                    kURLPathSeparator,
                    &path_components);

  if (path_components.size() < kProfileImageURLPathComponentsCount)
    return false;

  const std::string& photo_id = path_components[kPhotoIdPathComponentIndex];
  const std::string& photo_version =
      path_components[kPhotoVersionPathComponentIndex];

  // Check that the ID and version match the default Picasa profile photo.
  return photo_id == kPicasaPhotoId &&
         photo_version == kDefaultPicasaPhotoVersion;
}

ProfileDownloader::ProfileDownloader(ProfileDownloaderDelegate* delegate)
    : delegate_(delegate),
      picture_status_(PICTURE_FAILED) {
  DCHECK(delegate_);
}

void ProfileDownloader::Start() {
  VLOG(1) << "Starting profile downloader...";
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  TokenService* service =
      TokenServiceFactory::GetForProfile(delegate_->GetBrowserProfile());
  if (!service) {
    // This can happen in some test paths.
    LOG(WARNING) << "User has no token service";
    delegate_->OnProfileDownloadFailure(this);
    return;
  }

  if (service->HasOAuthLoginToken()) {
    StartFetchingOAuth2AccessToken();
  } else {
    registrar_.Add(this,
                   chrome::NOTIFICATION_TOKEN_AVAILABLE,
                   content::Source<TokenService>(service));
    registrar_.Add(this,
                   chrome::NOTIFICATION_TOKEN_REQUEST_FAILED,
                   content::Source<TokenService>(service));
  }
}

string16 ProfileDownloader::GetProfileFullName() const {
  return profile_full_name_;
}

SkBitmap ProfileDownloader::GetProfilePicture() const {
  return profile_picture_;
}

ProfileDownloader::PictureStatus ProfileDownloader::GetProfilePictureStatus()
    const {
  return picture_status_;
}

std::string ProfileDownloader::GetProfilePictureURL() const {
  return picture_url_;
}

void ProfileDownloader::StartFetchingImage() {
  VLOG(1) << "Fetching user entry with token: " << auth_token_;
  user_entry_fetcher_.reset(net::URLFetcher::Create(
      GURL(kUserEntryURL), net::URLFetcher::GET, this));
  user_entry_fetcher_->SetRequestContext(
      delegate_->GetBrowserProfile()->GetRequestContext());
  user_entry_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                                    net::LOAD_DO_NOT_SAVE_COOKIES);
  if (!auth_token_.empty()) {
    user_entry_fetcher_->SetExtraRequestHeaders(
        base::StringPrintf(kAuthorizationHeader, auth_token_.c_str()));
  }
  user_entry_fetcher_->Start();
}

void ProfileDownloader::StartFetchingOAuth2AccessToken() {
  TokenService* service =
      TokenServiceFactory::GetForProfile(delegate_->GetBrowserProfile());
  DCHECK(!service->GetOAuth2LoginRefreshToken().empty());

  std::vector<std::string> scopes;
  scopes.push_back(kAPIScope);
  oauth2_access_token_fetcher_.reset(new OAuth2AccessTokenFetcher(
      this, delegate_->GetBrowserProfile()->GetRequestContext()));
  oauth2_access_token_fetcher_->Start(
      GaiaUrls::GetInstance()->oauth2_chrome_client_id(),
      GaiaUrls::GetInstance()->oauth2_chrome_client_secret(),
      service->GetOAuth2LoginRefreshToken(),
      scopes);
}

ProfileDownloader::~ProfileDownloader() {}

void ProfileDownloader::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::string data;
  source->GetResponseAsString(&data);
  if (source->GetStatus().status() != net::URLRequestStatus::SUCCESS ||
      source->GetResponseCode() != 200) {
    LOG(WARNING) << "Fetching profile data failed";
    DVLOG(1) << "  Status: " << source->GetStatus().status();
    DVLOG(1) << "  Error: " << source->GetStatus().error();
    DVLOG(1) << "  Response code: " << source->GetResponseCode();
    DVLOG(1) << "  Url: " << source->GetURL().spec();
    delegate_->OnProfileDownloadFailure(this);
    return;
  }

  if (source == user_entry_fetcher_.get()) {
    std::string image_url;
    if (!GetProfileNameAndImageURL(data, &profile_full_name_, &image_url,
        delegate_->GetDesiredImageSideLength())) {
      delegate_->OnProfileDownloadFailure(this);
      return;
    }
    if (!delegate_->NeedsProfilePicture()) {
      VLOG(1) << "Skipping profile picture download";
      delegate_->OnProfileDownloadSuccess(this);
      return;
    }
    if (IsDefaultProfileImageURL(image_url)) {
      VLOG(1) << "User has default profile picture";
      picture_status_ = PICTURE_DEFAULT;
      delegate_->OnProfileDownloadSuccess(this);
      return;
    }
    if (!image_url.empty() && image_url == delegate_->GetCachedPictureURL()) {
      VLOG(1) << "Picture URL matches cached picture URL";
      picture_status_ = PICTURE_CACHED;
      delegate_->OnProfileDownloadSuccess(this);
      return;
    }
    VLOG(1) << "Fetching profile image from " << image_url;
    picture_url_ = image_url;
    profile_image_fetcher_.reset(net::URLFetcher::Create(
        GURL(image_url), net::URLFetcher::GET, this));
    profile_image_fetcher_->SetRequestContext(
        delegate_->GetBrowserProfile()->GetRequestContext());
    profile_image_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                                         net::LOAD_DO_NOT_SAVE_COOKIES);
    if (!auth_token_.empty()) {
      profile_image_fetcher_->SetExtraRequestHeaders(
          base::StringPrintf(kAuthorizationHeader, auth_token_.c_str()));
    }
    profile_image_fetcher_->Start();
  } else if (source == profile_image_fetcher_.get()) {
    VLOG(1) << "Decoding the image...";
    scoped_refptr<ImageDecoder> image_decoder = new ImageDecoder(
        this, data, ImageDecoder::DEFAULT_CODEC);
    image_decoder->Start();
  }
}

void ProfileDownloader::OnImageDecoded(const ImageDecoder* decoder,
                                       const SkBitmap& decoded_image) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  int image_size = delegate_->GetDesiredImageSideLength();
  profile_picture_ = skia::ImageOperations::Resize(
      decoded_image,
      skia::ImageOperations::RESIZE_BEST,
      image_size,
      image_size);
  picture_status_ = PICTURE_SUCCESS;
  delegate_->OnProfileDownloadSuccess(this);
}

void ProfileDownloader::OnDecodeImageFailed(const ImageDecoder* decoder) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  delegate_->OnProfileDownloadFailure(this);
}

void ProfileDownloader::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_TOKEN_AVAILABLE ||
         type == chrome::NOTIFICATION_TOKEN_REQUEST_FAILED);

  TokenService::TokenAvailableDetails* token_details =
      content::Details<TokenService::TokenAvailableDetails>(details).ptr();

  if (type == chrome::NOTIFICATION_TOKEN_AVAILABLE) {
    if (token_details->service() ==
        GaiaConstants::kGaiaOAuth2LoginRefreshToken) {
      registrar_.RemoveAll();
      StartFetchingOAuth2AccessToken();
    }
  } else {
    if (token_details->service() ==
        GaiaConstants::kGaiaOAuth2LoginRefreshToken) {
      LOG(WARNING) << "ProfileDownloader: token request failed";
      delegate_->OnProfileDownloadFailure(this);
    }
  }
}

// Callback for OAuth2AccessTokenFetcher on success. |access_token| is the token
// used to start fetching user data.
void ProfileDownloader::OnGetTokenSuccess(const std::string& access_token,
                                          const base::Time& expiration_time) {
  auth_token_ = access_token;
  StartFetchingImage();
}

// Callback for OAuth2AccessTokenFetcher on failure.
void ProfileDownloader::OnGetTokenFailure(const GoogleServiceAuthError& error) {
  LOG(WARNING) << "ProfileDownloader: token request using refresh token failed";
  delegate_->OnProfileDownloadFailure(this);
}

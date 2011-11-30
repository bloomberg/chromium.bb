// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_downloader_delegate.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/net/gaia/gaia_urls.h"
#include "chrome/common/net/gaia/oauth2_access_token_fetcher.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/url_fetcher.h"
#include "googleurl/src/gurl.h"
#include "skia/ext/image_operations.h"

using content::BrowserThread;

namespace {

// Template for optional authorization header when using the ClientLogin access
// token.
const char kClientAccessAuthorizationHeader[] =
    "Authorization: GoogleLogin auth=%s";

// Template for optional authorization header when using an OAuth access token.
const char kOAuthAccessAuthorizationHeader[] =
    "Authorization: Bearer %s";

// URL requesting Picasa API for user info.
const char kUserEntryURL[] =
    "http://picasaweb.google.com/data/entry/api/user/default?alt=json";

// OAuth scope for the Picasa API.
const char kPicasaScope[] = "http://picasaweb.google.com/data/";

// Path in JSON dictionary to user's photo thumbnail URL.
const char kPhotoThumbnailURLPath[] = "entry.gphoto$thumbnail.$t";

const char kNickNamePath[] = "entry.gphoto$nickname.$t";

// Path format for specifying thumbnail's size.
const char kThumbnailSizeFormat[] = "s%d-c";
// Default Picasa thumbnail size.
const int kDefaultThumbnailSize = 64;

// Separator of URL path components.
const char kURLPathSeparator = '/';

// Photo ID of the Picasa Web Albums profile picture (base64 of 0).
const char kPicasaPhotoId[] = "AAAAAAAAAAA";

// Photo version of the default PWA profile picture (base64 of 1).
const char kDefaultPicasaPhotoVersion[] = "AAAAAAAAAAE";

// Photo ID of the Google+ profile picture (base64 of 2).
const char kGooglePlusPhotoId[] = "AAAAAAAAAAI";

// Photo version of the default Google+ profile picture (base64 of 0).
const char kDefaultGooglePlusPhotoVersion[] = "AAAAAAAAAAA";

// Number of path components in profile picture URL.
const size_t kProfileImageURLPathComponentsCount = 7;

// Index of path component with photo ID.
const int kPhotoIdPathComponentIndex = 2;

// Index of path component with photo version.
const int kPhotoVersionPathComponentIndex = 3;

}  // namespace

bool ProfileDownloader::GetProfileNickNameAndImageURL(const std::string& data,
                                                      string16* nick_name,
                                                      std::string* url) const {
  DCHECK(nick_name);
  DCHECK(url);
  *nick_name = string16();
  *url = std::string();

  int error_code = -1;
  std::string error_message;
  scoped_ptr<base::Value> root_value(base::JSONReader::ReadAndReturnError(
      data, false, &error_code, &error_message));
  if (!root_value.get()) {
    LOG(ERROR) << "Error while parsing Picasa user entry response: "
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

  if (!root_dictionary->GetString(kNickNamePath, nick_name)) {
    LOG(ERROR) << "Can't find nick name path in JSON data: "
               << data;
    return false;
  }

  std::string thumbnail_url_string;
  if (!root_dictionary->GetString(
          kPhotoThumbnailURLPath, &thumbnail_url_string)) {
    LOG(ERROR) << "Can't find thumbnail path in JSON data: "
               << data;
    return false;
  }

  // Try to change the size of thumbnail we are going to get.
  // Typical URL looks like this:
  // http://lh0.ggpht.com/-abcd1aBCDEf/AAAA/AAA_A/abc12/s64-c/1234567890.jpg
  std::string default_thumbnail_size_path_component(
      base::StringPrintf(kThumbnailSizeFormat, kDefaultThumbnailSize));
  int image_size = delegate_->GetDesiredImageSideLength();
  std::string new_thumbnail_size_path_component(
      base::StringPrintf(kThumbnailSizeFormat, image_size));
  size_t thumbnail_size_pos =
      thumbnail_url_string.find(default_thumbnail_size_path_component);
  if (thumbnail_size_pos != std::string::npos) {
    size_t thumbnail_size_end =
        thumbnail_size_pos + default_thumbnail_size_path_component.size();
   thumbnail_url_string =
        thumbnail_url_string.substr(0, thumbnail_size_pos) +
        new_thumbnail_size_path_component +
        thumbnail_url_string.substr(
            thumbnail_size_end,
            thumbnail_url_string.size() - thumbnail_size_end);
  } else {
    LOG(WARNING) << "Hasn't found thumbnail size part in image URL: "
                 << thumbnail_url_string;
    // Use the thumbnail URL we have.
  }

  GURL thumbnail_url(thumbnail_url_string);
  if (!thumbnail_url.is_valid()) {
    LOG(ERROR) << "Thumbnail URL is not valid: " << thumbnail_url_string;
    return false;
  }
  *url = thumbnail_url.spec();
  return true;
}

bool ProfileDownloader::IsDefaultProfileImageURL(const std::string& url) const {
  GURL image_url_object(url);
  DCHECK(image_url_object.is_valid());
  VLOG(1) << "URL to check for default image: " << image_url_object.spec();
  std::vector<std::string> path_components;
  base::SplitString(image_url_object.path(),
                    kURLPathSeparator,
                    &path_components);

  if (path_components.size() != kProfileImageURLPathComponentsCount)
    return false;

  const std::string& photo_id = path_components[kPhotoIdPathComponentIndex];
  const std::string& photo_version =
      path_components[kPhotoVersionPathComponentIndex];

  // There are at least two pairs of (ID, version) for the default photo:
  // the default Google+ profile photo and the default Picasa profile photo.
  return ((photo_id == kPicasaPhotoId &&
           photo_version == kDefaultPicasaPhotoVersion) ||
          (photo_id == kGooglePlusPhotoId &&
           photo_version == kDefaultGooglePlusPhotoVersion));
}

ProfileDownloader::ProfileDownloader(ProfileDownloaderDelegate* delegate)
    : delegate_(delegate),
      picture_status_(PICTURE_FAILED) {
  DCHECK(delegate_);
}

void ProfileDownloader::Start() {
  VLOG(1) << "Starting profile downloader...";
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  TokenService* service = delegate_->GetBrowserProfile()->GetTokenService();
  if (!service) {
    // This can happen in some test paths.
    LOG(WARNING) << "User has no token service";
    delegate_->OnDownloadComplete(this, false);
    return;
  }

  if (delegate_->ShouldUseOAuthRefreshToken() &&
      service->HasOAuthLoginToken()) {
    StartFetchingOAuth2AccessToken();
  } else if (!delegate_->ShouldUseOAuthRefreshToken() &&
      service->HasTokenForService(GaiaConstants::kPicasaService)) {
    auth_token_ =
        service->GetTokenForService(GaiaConstants::kPicasaService);
    StartFetchingImage();
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
  user_entry_fetcher_.reset(content::URLFetcher::Create(
      GURL(kUserEntryURL), content::URLFetcher::GET, this));
  user_entry_fetcher_->SetRequestContext(
      delegate_->GetBrowserProfile()->GetRequestContext());
  if (!auth_token_.empty()) {
    user_entry_fetcher_->SetExtraRequestHeaders(
        base::StringPrintf(GetAuthorizationHeader(), auth_token_.c_str()));
  }
  user_entry_fetcher_->Start();
}

const char* ProfileDownloader::GetAuthorizationHeader() const {
  return delegate_->ShouldUseOAuthRefreshToken() ?
      kOAuthAccessAuthorizationHeader : kClientAccessAuthorizationHeader;
}

void ProfileDownloader::StartFetchingOAuth2AccessToken() {
  TokenService* service = delegate_->GetBrowserProfile()->GetTokenService();
  DCHECK(!service->GetOAuth2LoginRefreshToken().empty());

  std::vector<std::string> scopes;
  scopes.push_back(kPicasaScope);
  oauth2_access_token_fetcher_.reset(new OAuth2AccessTokenFetcher(
      this, delegate_->GetBrowserProfile()->GetRequestContext()));
  oauth2_access_token_fetcher_->Start(
      GaiaUrls::GetInstance()->oauth2_chrome_client_id(),
      GaiaUrls::GetInstance()->oauth2_chrome_client_secret(),
      service->GetOAuth2LoginRefreshToken(),
      scopes);
}

ProfileDownloader::~ProfileDownloader() {}

void ProfileDownloader::OnURLFetchComplete(const content::URLFetcher* source) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::string data;
  source->GetResponseAsString(&data);
  if (source->GetResponseCode() != 200) {
    LOG(ERROR) << "Response code is " << source->GetResponseCode();
    LOG(ERROR) << "Url is " << source->GetURL().spec();
    LOG(ERROR) << "Data is " << data;
    delegate_->OnDownloadComplete(this, false);
    return;
  }

  if (source == user_entry_fetcher_.get()) {
    std::string image_url;
    if (!GetProfileNickNameAndImageURL(data, &profile_full_name_, &image_url)) {
      delegate_->OnDownloadComplete(this, false);
      return;
    }
    if (IsDefaultProfileImageURL(image_url)) {
      VLOG(1) << "User has default profile picture";
      picture_status_ = PICTURE_DEFAULT;
      delegate_->OnDownloadComplete(this, true);
      return;
    }
    if (!image_url.empty() && image_url == delegate_->GetCachedPictureURL()) {
      VLOG(1) << "Picture URL matches cached picture URL";
      picture_status_ = PICTURE_CACHED;
      delegate_->OnDownloadComplete(this, true);
      return;
    }
    VLOG(1) << "Fetching profile image from " << image_url;
    picture_url_ = image_url;
    profile_image_fetcher_.reset(content::URLFetcher::Create(
        GURL(image_url), content::URLFetcher::GET, this));
    profile_image_fetcher_->SetRequestContext(
        delegate_->GetBrowserProfile()->GetRequestContext());
    if (!auth_token_.empty()) {
      profile_image_fetcher_->SetExtraRequestHeaders(
          base::StringPrintf(GetAuthorizationHeader(), auth_token_.c_str()));
    }
    profile_image_fetcher_->Start();
  } else if (source == profile_image_fetcher_.get()) {
    VLOG(1) << "Decoding the image...";
    scoped_refptr<ImageDecoder> image_decoder = new ImageDecoder(
        this, data);
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
  delegate_->OnDownloadComplete(this, true);
}

void ProfileDownloader::OnDecodeImageFailed(const ImageDecoder* decoder) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  delegate_->OnDownloadComplete(this, false);
}

void ProfileDownloader::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_TOKEN_AVAILABLE ||
         type == chrome::NOTIFICATION_TOKEN_REQUEST_FAILED);

  TokenService::TokenAvailableDetails* token_details =
      content::Details<TokenService::TokenAvailableDetails>(details).ptr();
  std::string service = delegate_->ShouldUseOAuthRefreshToken() ?
      GaiaConstants::kGaiaOAuth2LoginRefreshToken :
      GaiaConstants::kPicasaService;

  if (type == chrome::NOTIFICATION_TOKEN_AVAILABLE) {
    if (token_details->service() == service) {
      registrar_.RemoveAll();
      if (delegate_->ShouldUseOAuthRefreshToken()) {
        StartFetchingOAuth2AccessToken();
      } else {
        auth_token_ = token_details->token();
        StartFetchingImage();
      }
    }
  } else {
    if (token_details->service() == service) {
      LOG(WARNING) << "ProfileDownloader: token request failed";
      delegate_->OnDownloadComplete(this, false);
    }
  }
}

// Callback for OAuth2AccessTokenFetcher on success. |access_token| is the token
// used to start fetching user data.
void ProfileDownloader::OnGetTokenSuccess(const std::string& access_token) {
  auth_token_ = access_token;
  StartFetchingImage();
}

// Callback for OAuth2AccessTokenFetcher on failure.
void ProfileDownloader::OnGetTokenFailure(const GoogleServiceAuthError& error) {
  LOG(WARNING) << "ProfileDownloader: token request using refresh token failed";
  delegate_->OnDownloadComplete(this, false);
}

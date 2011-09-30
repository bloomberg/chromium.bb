// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/profile_image_downloader.h"

#include <vector>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "content/browser/browser_thread.h"
#include "content/common/content_notification_types.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_service.h"
#include "content/common/notification_source.h"
#include "googleurl/src/gurl.h"
#include "skia/ext/image_operations.h"

namespace chromeos {

namespace {

// Template for optional authorization header.
const char kAuthorizationHeader[] = "Authorization: GoogleLogin auth=%s";

// URL requesting Picasa API for user info.
const char kUserEntryURL[] =
    "http://picasaweb.google.com/data/entry/api/user/default?alt=json";
// Path in JSON dictionary to user's photo thumbnail URL.
const char kPhotoThumbnailURLPath[] = "entry.gphoto$thumbnail.$t";
// Path format for specifying thumbnail's size.
const char kThumbnailSizeFormat[] = "s%d-c";
// Default Picasa thumbnail size.
const int kDefaultThumbnailSize = 64;

}  // namespace

std::string ProfileImageDownloader::GetProfileImageURL(
    const std::string& data) const {
  int error_code = -1;
  std::string error_message;
  scoped_ptr<base::Value> root_value(base::JSONReader::ReadAndReturnError(
      data, false, &error_code, &error_message));
  if (!root_value.get()) {
    LOG(ERROR) << "Error while parsing Picasa user entry response: "
               << error_message;
    return std::string();
  }
  if (!root_value->IsType(base::Value::TYPE_DICTIONARY)) {
    LOG(ERROR) << "JSON root is not a dictionary: "
               << root_value->GetType();
    return std::string();
  }
  base::DictionaryValue* root_dictionary =
      static_cast<base::DictionaryValue*>(root_value.get());

  std::string thumbnail_url_string;
  if (!root_dictionary->GetString(
          kPhotoThumbnailURLPath, &thumbnail_url_string)) {
    LOG(ERROR) << "Can't find thumbnail path in JSON data: "
               << data;
    return std::string();
  }

  // Try to change the size of thumbnail we are going to get.
  // Typical URL looks like this:
  // http://lh0.ggpht.com/-abcd1aBCDEf/AAAA/AAA_A/abc12/s64-c/1234567890.jpg
  std::string default_thumbnail_size_path_component(
      base::StringPrintf(kThumbnailSizeFormat, kDefaultThumbnailSize));
  std::string new_thumbnail_size_path_component(
      base::StringPrintf(kThumbnailSizeFormat, login::kUserImageSize));
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
    return std::string();
  }
  return thumbnail_url.spec();
}

ProfileImageDownloader::ProfileImageDownloader(Delegate* delegate)
    : delegate_(delegate) {
}

void ProfileImageDownloader::Start() {
  VLOG(1) << "Starting profile image downloader...";
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  TokenService* service =
      ProfileManager::GetDefaultProfile()->GetTokenService();
  if (!service) {
    // This can happen in some test paths.
    LOG(WARNING) << "User has no token service";
    if (delegate_)
      delegate_->OnDownloadFailure();
    return;
  }
  if (service->HasTokenForService(GaiaConstants::kPicasaService)) {
    auth_token_ =
        service->GetTokenForService(GaiaConstants::kPicasaService);
    StartFetchingImage();
  } else {
    registrar_.Add(this,
                   chrome::NOTIFICATION_TOKEN_AVAILABLE,
                   Source<TokenService>(service));
    registrar_.Add(this,
                   chrome::NOTIFICATION_TOKEN_REQUEST_FAILED,
                   Source<TokenService>(service));
  }
}

void ProfileImageDownloader::StartFetchingImage() {
  std::string email = UserManager::Get()->logged_in_user().email();
  if (email.empty())
    return;
  VLOG(1) << "Fetching user entry with token: " << auth_token_;
  user_entry_fetcher_.reset(
      new URLFetcher(GURL(kUserEntryURL), URLFetcher::GET, this));
  user_entry_fetcher_->set_request_context(
      ProfileManager::GetDefaultProfile()->GetRequestContext());
  if (!auth_token_.empty()) {
    user_entry_fetcher_->set_extra_request_headers(
        base::StringPrintf(kAuthorizationHeader, auth_token_.c_str()));
  }
  user_entry_fetcher_->Start();
}

ProfileImageDownloader::~ProfileImageDownloader() {}

void ProfileImageDownloader::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const net::ResponseCookies& cookies,
    const std::string& data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (response_code != 200) {
    LOG(ERROR) << "Response code is " << response_code;
    LOG(ERROR) << "Url is " << url.spec();
    LOG(ERROR) << "Data is " << data;
    if (delegate_)
      delegate_->OnDownloadFailure();
    return;
  }

  if (source == user_entry_fetcher_.get()) {
    std::string image_url = GetProfileImageURL(data);
    if (image_url.empty()) {
      if (delegate_)
        delegate_->OnDownloadFailure();
      return;
    }
    VLOG(1) << "Fetching profile image...";
    profile_image_fetcher_.reset(
        new URLFetcher(GURL(image_url), URLFetcher::GET, this));
    profile_image_fetcher_->set_request_context(
        ProfileManager::GetDefaultProfile()->GetRequestContext());
    if (!auth_token_.empty()) {
      profile_image_fetcher_->set_extra_request_headers(
          base::StringPrintf(kAuthorizationHeader, auth_token_.c_str()));
    }
    profile_image_fetcher_->Start();
  } else if (source == profile_image_fetcher_.get()) {
    VLOG(1) << "Decoding the image...";
    scoped_refptr<ImageDecoder> image_decoder = new ImageDecoder(this, data);
    image_decoder->Start();
  }
}

void ProfileImageDownloader::OnImageDecoded(const ImageDecoder* decoder,
                                            const SkBitmap& decoded_image) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SkBitmap resized_image = skia::ImageOperations::Resize(
      decoded_image,
      skia::ImageOperations::RESIZE_BEST,
      login::kUserImageSize,
      login::kUserImageSize);
  if (delegate_)
    delegate_->OnDownloadSuccess(resized_image);
}

void ProfileImageDownloader::OnDecodeImageFailed(const ImageDecoder* decoder) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (delegate_)
    delegate_->OnDownloadFailure();
}

void ProfileImageDownloader::Observe(int type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_TOKEN_AVAILABLE ||
         type == chrome::NOTIFICATION_TOKEN_REQUEST_FAILED);

  TokenService::TokenAvailableDetails* token_details =
      Details<TokenService::TokenAvailableDetails>(details).ptr();
  if (type == chrome::NOTIFICATION_TOKEN_AVAILABLE) {
    if (token_details->service() == GaiaConstants::kPicasaService) {
      registrar_.RemoveAll();
      auth_token_ = token_details->token();
      StartFetchingImage();
    }
  } else {
    if (token_details->service() == GaiaConstants::kPicasaService) {
      LOG(WARNING) << "ProfileImageDownloader: token request failed";
      if (delegate_)
        delegate_->OnDownloadFailure();
    }
  }
}

}  // namespace chromeos

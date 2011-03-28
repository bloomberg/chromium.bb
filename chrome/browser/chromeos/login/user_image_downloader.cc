// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_image_downloader.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/browser/chromeos/login/image_downloader.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/net/url_fetcher.h"
#include "content/browser/browser_thread.h"
#include "googleurl/src/gurl.h"

namespace chromeos {

namespace {

// Contacts API URL that returns all user info.
// TODO(avayvod): Find the way to receive less data for the user.
const char kUserInfoURL[] =
    "http://www.google.com/m8/feeds/contacts/default/thin?alt=json";

// Template for authorization header needed for all request to Contacts API.
const char kAuthorizationHeader[] = "Authorization: GoogleLogin auth=%s";

// Schema that identifies JSON node with image url.
const char kPhotoSchemaURL[] =
    "http://schemas.google.com/contacts/2008/rel#photo";

}  // namespace

UserImageDownloader::UserImageDownloader(const std::string& username,
                                         const std::string& auth_token)
    : username_(username),
      auth_token_(auth_token) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (auth_token_.empty())
    return;

  profile_fetcher_.reset(new URLFetcher(GURL(kUserInfoURL),
                                        URLFetcher::GET,
                                        this));
  profile_fetcher_->set_request_context(
      ProfileManager::GetDefaultProfile()->GetRequestContext());
  profile_fetcher_->set_extra_request_headers(
      base::StringPrintf(kAuthorizationHeader, auth_token_.c_str()));
  profile_fetcher_->Start();
}

UserImageDownloader::~UserImageDownloader() {
}

void UserImageDownloader::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (response_code != 200) {
    LOG(ERROR) << "Response code is " << response_code;
    LOG(ERROR) << "Url is " << url.spec();
    LOG(ERROR) << "Data is " << data;
    return;
  }

  if (source == profile_fetcher_.get()) {
    GURL image_url;
    if (!GetImageURL(data, &image_url)) {
      LOG(ERROR) << "Didn't find image url in " << data;
      return;
    }
    VLOG(1) << "Sending request to " << image_url;
    new ImageDownloader(this, GURL(image_url), auth_token_);
  }
}

void UserImageDownloader::OnImageDecoded(const SkBitmap& decoded_image) {
  // Save the image to file and its path to preferences.
  chromeos::UserManager* user_manager = chromeos::UserManager::Get();
  if (user_manager) {
    if (user_manager->logged_in_user().email() == username_) {
      user_manager->SetLoggedInUserImage(decoded_image);
    }
    user_manager->SaveUserImage(username_, decoded_image);
  }
}

bool UserImageDownloader::GetImageURL(const std::string& json_data,
                                      GURL* image_url) const {
  if (!image_url) {
    NOTREACHED();
    return false;
  }

  // Data is in JSON format with image url located at the following path:
  // root > feed > entry > dictionary > link > dictionary > href.
  scoped_ptr<Value> root(base::JSONReader::Read(json_data, true));
  if (!root.get() || root->GetType() != Value::TYPE_DICTIONARY)
    return false;

  DictionaryValue* root_dictionary =
      static_cast<DictionaryValue*>(root.get());
  DictionaryValue* feed_dictionary = NULL;
  if (!root_dictionary->GetDictionary("feed", &feed_dictionary))
    return false;

  ListValue* entry_list = NULL;
  if (!feed_dictionary->GetList("entry", &entry_list))
    return false;

  return GetImageURLFromEntries(entry_list, image_url);
}

bool UserImageDownloader::GetImageURLFromEntries(ListValue* entry_list,
                                                 GURL* image_url) const {
  // The list contains info about all user's contacts including user
  // himself. We need to find entry for the user and then get his image.
  for (size_t i = 0; i < entry_list->GetSize(); ++i) {
    DictionaryValue* entry_dictionary = NULL;
    if (!entry_list->GetDictionary(i, &entry_dictionary))
      continue;

    ListValue* email_list = NULL;
    if (!entry_dictionary->GetList("gd$email", &email_list))
      continue;

    // Match entry email address to understand that this is user's entry.
    if (!IsUserEntry(email_list))
      continue;

    ListValue* link_list = NULL;
    if (!entry_dictionary->GetList("link", &link_list))
      continue;

    if (GetImageURLFromLinks(link_list, image_url))
      return true;
  }

  return false;
}

bool UserImageDownloader::IsUserEntry(ListValue* email_list) const {
  for (size_t i = 0; i < email_list->GetSize(); ++i) {
    DictionaryValue* email_dictionary = NULL;
    if (!email_list->GetDictionary(i, &email_dictionary))
      continue;

    std::string email;
    if (!email_dictionary->GetStringASCII("address", &email))
      continue;

    if (Authenticator::Canonicalize(email) == username_)
      return true;
  }
  return false;
}

bool UserImageDownloader::GetImageURLFromLinks(ListValue* link_list,
                                               GURL* image_url) const {
  // In entry's list of links there should be one with rel pointing to photo
  // schema.
  for (size_t i = 0; i < link_list->GetSize(); ++i) {
    DictionaryValue* link_dictionary = NULL;
    if (!link_list->GetDictionary(i, &link_dictionary))
      continue;

    std::string rel;
    if (!link_dictionary->GetStringASCII("rel", &rel))
      continue;

    if (rel != kPhotoSchemaURL)
      continue;

    std::string url;
    if (!link_dictionary->GetStringASCII("href", &url))
      continue;

    *image_url = GURL(url);
    return true;
  }
  return false;
}

}  // namespace chromeos

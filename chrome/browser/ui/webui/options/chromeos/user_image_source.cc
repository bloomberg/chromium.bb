// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/user_image_source.h"

#include "base/memory/ref_counted_memory.h"
#include "base/message_loop.h"
#include "base/string_split.h"
#include "chrome/browser/chromeos/login/default_user_images.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/ui/webui/web_ui_util.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/url_parse.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"

namespace {

// Animated key is used in user image URL requests to specify that
// animated version of user image is required. Without that key
// non-animated version of user image should be returned.
const char kKeyAnimated[] = "animated";

// Parses the user image URL, which looks like
// "chrome://userimage/user@host?key1=value1&...&key_n=value_n@<scale>x",
// to user email, optional parameters and scale factor.
void ParseRequest(const GURL& url,
                  std::string* email,
                  bool* is_image_animated,
                  ui::ScaleFactor* scale_factor) {
  DCHECK(url.is_valid());
  web_ui_util::ParsePathAndScale(url, email, scale_factor);
  std::string url_spec = url.possibly_invalid_spec();
  url_parse::Component query = url.parsed_for_possibly_invalid_spec().query;
  url_parse::Component key, value;
  *is_image_animated = false;
  while (ExtractQueryKeyValue(url_spec.c_str(), &query, &key, &value)) {
    if (url_spec.substr(key.begin, key.len) == kKeyAnimated) {
      *is_image_animated = true;
      break;
    }
  }
}

}  // namespace

namespace chromeos {
namespace options {

base::RefCountedMemory* UserImageSource::GetUserImage(
    const std::string& email,
    bool is_image_animated,
    ui::ScaleFactor scale_factor) const {
  const chromeos::User* user = chromeos::UserManager::Get()->FindUser(email);
  if (user) {
    if (user->has_animated_image() && is_image_animated) {
      return new base::RefCountedBytes(user->animated_image());
    } else if (user->has_raw_image()) {
      return new base::RefCountedBytes(user->raw_image());
    } else if (user->image_is_stub()) {
      return ResourceBundle::GetSharedInstance().
          LoadDataResourceBytesForScale(IDR_PROFILE_PICTURE_LOADING,
                                        scale_factor);
    } else if (user->HasDefaultImage()) {
      return ResourceBundle::GetSharedInstance().
          LoadDataResourceBytesForScale(
              kDefaultImageResourceIDs[user->image_index()],
              scale_factor);
    } else {
      NOTREACHED() << "User with custom image missing raw data";
    }
  }
  return ResourceBundle::GetSharedInstance().
      LoadDataResourceBytesForScale(IDR_LOGIN_DEFAULT_USER, scale_factor);
}

UserImageSource::UserImageSource() {
}

UserImageSource::~UserImageSource() {}

std::string UserImageSource::GetSource() {
  return chrome::kChromeUIUserImageHost;
}

void UserImageSource::StartDataRequest(
    const std::string& path,
    bool is_incognito,
    const content::URLDataSource::GotDataCallback& callback) {
  std::string email;
  bool is_image_animated = false;
  ui::ScaleFactor scale_factor;
  GURL url(chrome::kChromeUIUserImageURL + path);
  ParseRequest(url, &email, &is_image_animated, &scale_factor);
  callback.Run(GetUserImage(email, is_image_animated, scale_factor));
}

std::string UserImageSource::GetMimeType(const std::string& path) const {
  // We need to explicitly return a mime type, otherwise if the user tries to
  // drag the image they get no extension.
  std::string email;
  bool is_image_animated = false;
  ui::ScaleFactor scale_factor;

  GURL url(chrome::kChromeUIUserImageURL + path);
  ParseRequest(url, &email, &is_image_animated, &scale_factor);

  if (is_image_animated) {
    const chromeos::User* user = chromeos::UserManager::Get()->FindUser(email);
    if (user && user->has_animated_image())
      return "image/gif";
  }
  return "image/png";
}

}  // namespace options
}  // namespace chromeos

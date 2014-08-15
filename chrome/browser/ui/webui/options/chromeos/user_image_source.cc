// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/user_image_source.h"

#include "base/memory/ref_counted_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_split.h"
#include "chrome/common/url_constants.h"
#include "components/user_manager/user_image/default_user_images.h"
#include "components/user_manager/user_manager.h"
#include "grit/theme_resources.h"
#include "grit/ui_chromeos_resources.h"
#include "net/base/escape.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/url_parse.h"

namespace {

// Parses the user image URL, which looks like
// "chrome://userimage/user@host?key1=value1&...&key_n=value_n",
// to user email.
void ParseRequest(const GURL& url,
                  std::string* email) {
  DCHECK(url.is_valid());
  *email = net::UnescapeURLComponent(url.path().substr(1),
                                    (net::UnescapeRule::URL_SPECIAL_CHARS |
                                     net::UnescapeRule::SPACES));
}

}  // namespace

namespace chromeos {
namespace options {

base::RefCountedMemory* UserImageSource::GetUserImage(
    const std::string& email,
    ui::ScaleFactor scale_factor) const {
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(email);
  if (user) {
    if (user->has_raw_image()) {
      return new base::RefCountedBytes(user->raw_image());
    } else if (user->image_is_stub()) {
      return ResourceBundle::GetSharedInstance().
          LoadDataResourceBytesForScale(IDR_PROFILE_PICTURE_LOADING,
                                        scale_factor);
    } else if (user->HasDefaultImage()) {
      return ResourceBundle::GetSharedInstance().
          LoadDataResourceBytesForScale(
              user_manager::kDefaultImageResourceIDs[user->image_index()],
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

std::string UserImageSource::GetSource() const {
  return chrome::kChromeUIUserImageHost;
}

void UserImageSource::StartDataRequest(
    const std::string& path,
    int render_process_id,
    int render_frame_id,
    const content::URLDataSource::GotDataCallback& callback) {
  std::string email;
  GURL url(chrome::kChromeUIUserImageURL + path);
  ParseRequest(url, &email);
  callback.Run(GetUserImage(email, ui::SCALE_FACTOR_100P));
}

std::string UserImageSource::GetMimeType(const std::string& path) const {
  // We need to explicitly return a mime type, otherwise if the user tries to
  // drag the image they get no extension.
  return "image/png";
}

}  // namespace options
}  // namespace chromeos

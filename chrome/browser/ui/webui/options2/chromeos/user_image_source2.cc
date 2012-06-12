// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options2/chromeos/user_image_source2.h"

#include "base/memory/ref_counted_memory.h"
#include "base/message_loop.h"
#include "base/string_split.h"
#include "chrome/browser/chromeos/login/user_manager.h"
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

// Extracts from user image request user email and type of requested
// image (animated or non-animated). |path| is an user image request
// and should look like "username@host?key1=value1&...&key_n=value_n".
// So, "username@host" is stored into |email|. If a query part of
// |path| contains "animated" key, |is_image_animated| is set to true,
// otherwise |is_image_animated| is set to false. Doesn't change
// arguments if email can't be parsed (for instance, in guest mode).
void ParseRequest(const std::string& path,
                  std::string* email,
                  bool* is_image_animated) {
  url_parse::Parsed parsed;
  url_parse::ParseStandardURL(path.c_str(), path.size(), &parsed);
  if (!parsed.username.is_valid() || !parsed.host.is_valid())
    return;

  DCHECK(email != NULL);
  *email = path.substr(parsed.username.begin, parsed.username.len);
  email->append("@");
  email->append(path.substr(parsed.host.begin, parsed.host.len));

  if (!parsed.query.is_valid())
    return;

  url_parse::Component query = parsed.query;
  url_parse::Component key, value;
  DCHECK(is_image_animated != NULL);
  *is_image_animated = false;
  while (ExtractQueryKeyValue(path.c_str(), &query, &key, &value)) {
    if (path.substr(key.begin, key.len) == kKeyAnimated) {
      *is_image_animated = true;
      break;
    }
  }
}

}  // namespace

namespace chromeos {
namespace options2 {

std::vector<unsigned char> UserImageSource::GetUserImage(
    const std::string& email, bool is_image_animated) const {
  std::vector<unsigned char> user_image;
  const chromeos::User* user = chromeos::UserManager::Get()->FindUser(email);
  if (user) {
    if (user->has_animated_image() && is_image_animated)
      user->GetAnimatedImage(&user_image);
    else
      gfx::PNGCodec::EncodeBGRASkBitmap(user->image(), false, &user_image);
    return user_image;
  }
  gfx::PNGCodec::EncodeBGRASkBitmap(
      *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_LOGIN_DEFAULT_USER),
      false,
      &user_image);
  return user_image;
}

UserImageSource::UserImageSource()
    : DataSource(chrome::kChromeUIUserImageHost, MessageLoop::current()) {
}

UserImageSource::~UserImageSource() {}

void UserImageSource::StartDataRequest(const std::string& path,
                                       bool is_incognito,
                                       int request_id) {
  std::string email;
  bool is_image_animated = false;
  ParseRequest(path, &email, &is_image_animated);

  std::vector<unsigned char> image = GetUserImage(email, is_image_animated);
  SendResponse(request_id, new base::RefCountedBytes(image));
}

std::string UserImageSource::GetMimeType(const std::string& path) const {
  // We need to explicitly return a mime type, otherwise if the user tries to
  // drag the image they get no extension.
  std::string email;
  bool is_image_animated = false;
  ParseRequest(path, &email, &is_image_animated);

  if (is_image_animated) {
    const chromeos::User* user = chromeos::UserManager::Get()->FindUser(email);
    if (user && user->has_animated_image())
      return "image/gif";
  }
  return "image/png";
}

}  // namespace options2
}  // namespace chromeos

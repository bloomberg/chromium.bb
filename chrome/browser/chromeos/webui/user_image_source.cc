// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/webui/user_image_source.h"

#include "base/ref_counted_memory.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/common/url_constants.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"

namespace chromeos {

std::vector<unsigned char> UserImageSource::GetUserImage(
    const std::string& email) const {
  std::vector<unsigned char> user_image;
  chromeos::UserVector users = chromeos::UserManager::Get()->GetUsers();
  for (size_t i = 0; i < users.size(); ++i) {
    if (users[i].email() == email) {
      gfx::PNGCodec::EncodeBGRASkBitmap(users[i].image(), true, &user_image);
      return user_image;
    }
  }
  gfx::PNGCodec::EncodeBGRASkBitmap(
      *ResourceBundle::GetSharedInstance().GetBitmapNamed(
          IDR_LOGIN_DEFAULT_USER),
      true,
      &user_image);
  return user_image;
}

UserImageSource::UserImageSource()
    : DataSource(chrome::kChromeUIUserImageHost, MessageLoop::current()) {
}

UserImageSource::~UserImageSource() {}

void UserImageSource::StartDataRequest(const std::string& path,
                                       bool is_off_the_record,
                                       int request_id) {
  SendResponse(request_id, new RefCountedBytes(GetUserImage(path)));
}

std::string UserImageSource::GetMimeType(const std::string&) const {
  // We need to explicitly return a mime type, otherwise if the user tries to
  // drag the image they get no extension.
  return "image/png";
}

}  // namespace chromeos

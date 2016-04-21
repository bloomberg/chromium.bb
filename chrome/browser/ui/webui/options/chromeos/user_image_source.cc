// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/user_image_source.h"

#include "base/memory/ref_counted_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_split.h"
#include "chrome/browser/chromeos/login/users/default_user_image/default_user_images.h"
#include "chrome/common/url_constants.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "grit/theme_resources.h"
#include "grit/ui_chromeos_resources.h"
#include "net/base/escape.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/third_party/mozilla/url_parse.h"

namespace {

// Parses the user image URL, which looks like
// "chrome://userimage/serialized-user-id?key1=value1&...&key_n=value_n",
// to user email.
void ParseRequest(const GURL& url, std::string* email) {
  DCHECK(url.is_valid());
  const std::string serialized_account_id = net::UnescapeURLComponent(
      url.path().substr(1),
      net::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS |
          net::UnescapeRule::PATH_SEPARATORS | net::UnescapeRule::SPACES);
  AccountId account_id(EmptyAccountId());
  const bool status =
      AccountId::Deserialize(serialized_account_id, &account_id);
  // TODO(alemate): DCHECK(status) - should happen after options page is
  // migrated.
  if (!status) {
    LOG(WARNING) << "Failed to deserialize account_id.";
    account_id = user_manager::known_user::GetAccountId(
        serialized_account_id, std::string() /* gaia_id */);
  }
  *email = account_id.GetUserEmail();
}

base::RefCountedMemory* GetUserImageInternal(const AccountId& account_id) {
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id);

  // Always use the 100% scaling. These source images are 256x256, and are
  // downscaled to ~64x64 for use in WebUI pages. Therefore, they are big enough
  // for device scale factors up to 4. We do not use SCALE_FACTOR_NONE, as we
  // specifically want 100% scale images to not transmit more data than needed.
  if (user) {
    if (user->has_image_bytes()) {
      return new base::RefCountedBytes(user->image_bytes());
    } else if (user->image_is_stub()) {
      return ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
          IDR_PROFILE_PICTURE_LOADING, ui::SCALE_FACTOR_100P);
    } else if (user->HasDefaultImage()) {
      return ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
          chromeos::default_user_image::kDefaultImageResourceIDs
              [user->image_index()],
          ui::SCALE_FACTOR_100P);
    } else {
      NOTREACHED() << "User with custom image missing data bytes";
    }
  }
  return ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
      IDR_LOGIN_DEFAULT_USER, ui::SCALE_FACTOR_100P);
}

}  // namespace

namespace chromeos {
namespace options {

// Static.
scoped_refptr<base::RefCountedMemory> UserImageSource::GetUserImage(
    const AccountId& account_id) {
  return GetUserImageInternal(account_id);
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
  const AccountId account_id(AccountId::FromUserEmail(email));
  callback.Run(GetUserImageInternal(account_id));
}

std::string UserImageSource::GetMimeType(const std::string& path) const {
  // We need to explicitly return a mime type, otherwise if the user tries to
  // drag the image they get no extension.
  return "image/png";
}

}  // namespace options
}  // namespace chromeos

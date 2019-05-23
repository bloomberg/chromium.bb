// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/session/user_info.h"

namespace ash {

UserAvatar::UserAvatar() = default;
UserAvatar::UserAvatar(const UserAvatar& other) = default;
UserAvatar::~UserAvatar() = default;

bool operator==(const UserAvatar& a, const UserAvatar& b) {
  return a.image.BackedBySameObjectAs(b.image) && a.bytes == b.bytes;
}

UserInfo::UserInfo() = default;
UserInfo::UserInfo(const UserInfo& other) = default;
UserInfo::~UserInfo() = default;

mojom::UserInfoPtr UserInfo::ToMojom() const {
  mojom::UserInfoPtr user_info = mojom::UserInfo::New();
  user_info->type = type;
  user_info->account_id = account_id;
  user_info->service_instance_group = service_instance_group;
  user_info->display_name = display_name;
  user_info->display_email = display_email;

  user_info->avatar = mojom::UserAvatar::New();
  user_info->avatar->image = avatar.image;
  user_info->avatar->bytes = avatar.bytes;

  user_info->is_new_profile = is_new_profile;
  user_info->is_ephemeral = is_ephemeral;
  user_info->is_device_owner = is_device_owner;
  user_info->has_gaia_account = has_gaia_account;
  user_info->should_display_managed_ui = should_display_managed_ui;
  return user_info;
}

ASH_PUBLIC_EXPORT bool operator==(const UserInfo& a, const UserInfo& b) {
  return a.type == b.type && a.account_id == b.account_id &&
         a.service_instance_group == b.service_instance_group &&
         a.display_name == b.display_name &&
         a.display_email == b.display_email && a.avatar == b.avatar &&
         a.is_new_profile == b.is_new_profile &&
         a.is_ephemeral == b.is_ephemeral &&
         a.is_device_owner == b.is_device_owner &&
         a.has_gaia_account == b.has_gaia_account &&
         a.should_display_managed_ui == b.should_display_managed_ui;
}

}  // namespace ash

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/users/avatar/user_image_manager.h"

namespace chromeos {

UserImageManager::UserImageManager(const std::string& user_id)
    : user_id_(user_id) {}

UserImageManager::~UserImageManager() {}

}  // namespace chromeos

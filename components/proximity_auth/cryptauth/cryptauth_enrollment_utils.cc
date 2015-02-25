// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/cryptauth/cryptauth_enrollment_utils.h"

#include "base/sha1.h"
#include "components/proximity_auth/base64url.h"

namespace proximity_auth {

std::string CalculateDeviceUserId(const std::string& device_id,
                                  const std::string& user_id) {
  std::string device_user_id;
  Base64UrlEncode(base::SHA1HashString(device_id + "|" + user_id),
                  &device_user_id);
  return device_user_id;
}

}  // namespace proximity_auth

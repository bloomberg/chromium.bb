// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_CRYPT_AUTH_ENROLLMENT_UTILS_H
#define COMPONENTS_PROXIMITY_AUTH_CRYPT_AUTH_ENROLLMENT_UTILS_H

#include <string>

namespace proximity_auth {

// Calculates an id for the specified user on the specified device.
std::string CalculateDeviceUserId(const std::string& device_id,
                                  const std::string& user_id);

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_CRYPT_AUTH_ENROLLMENT_UTILS_H

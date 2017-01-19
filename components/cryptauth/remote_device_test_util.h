// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRYPTAUTH_REMOTE_DEVICE_TEST_UTIL_H_
#define COMPONENTS_CRYPTAUTH_REMOTE_DEVICE_TEST_UTIL_H_

#include <vector>

#include "components/cryptauth/remote_device.h"

namespace cryptauth {

std::vector<RemoteDevice> GenerateTestRemoteDevices(size_t num_to_create);

}  // namespace cryptauth

#endif  // COMPONENTS_CRYPTAUTH_REMOTE_DEVICE_TEST_UTIL_H_

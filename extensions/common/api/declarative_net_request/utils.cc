// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/declarative_net_request/utils.h"

#include "components/version_info/channel.h"
#include "extensions/common/features/feature_channel.h"

namespace extensions {
namespace declarative_net_request {

namespace {

constexpr version_info::Channel kAPIChannel = version_info::Channel::UNKNOWN;

}  // namespace

bool IsAPIAvailable() {
  // TODO(http://crbug.com/748309): Use Feature::IsAvailableToEnvironment() once
  // it behaves correctly.
  return kAPIChannel >= GetCurrentChannel();
}

}  // namespace declarative_net_request
}  // namespace extensions

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/platform_channel_pair.h"

#include "base/logging.h"

namespace mojo {
namespace system {

PlatformChannelPair::~PlatformChannelPair() {
}

ScopedPlatformHandle PlatformChannelPair::PassServerHandle() {
  return server_handle_.Pass();
}

ScopedPlatformHandle PlatformChannelPair::PassClientHandle() {
  return client_handle_.Pass();
}

}  // namespace system
}  // namespace mojo

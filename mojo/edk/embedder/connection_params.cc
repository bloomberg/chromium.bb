// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/embedder/connection_params.h"

#include <utility>

namespace mojo {
namespace edk {

ConnectionParams::ConnectionParams(ScopedPlatformHandle channel)
    : channel_(std::move(channel)) {}

ConnectionParams::ConnectionParams(ConnectionParams&& param)
    : channel_(std::move(param.channel_)) {}

ConnectionParams& ConnectionParams::operator=(ConnectionParams&& param) {
  channel_ = std::move(param.channel_);
  return *this;
}

ScopedPlatformHandle ConnectionParams::TakeChannelHandle() {
  return std::move(channel_);
}

}  // namespace edk
}  // namespace mojo

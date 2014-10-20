// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/features/feature_channel.h"

using chrome::VersionInfo;

namespace {

const VersionInfo::Channel kDefaultChannel = VersionInfo::CHANNEL_STABLE;
VersionInfo::Channel g_current_channel = kDefaultChannel;

}  // namespace

namespace extensions {

VersionInfo::Channel GetCurrentChannel() {
  return g_current_channel;
}

void SetCurrentChannel(VersionInfo::Channel channel) {
  g_current_channel = channel;
}

VersionInfo::Channel GetDefaultChannel() {
  return kDefaultChannel;
}

ScopedCurrentChannel::ScopedCurrentChannel(VersionInfo::Channel channel)
    : original_channel_(VersionInfo::CHANNEL_UNKNOWN) {
  original_channel_ = GetCurrentChannel();
  SetCurrentChannel(channel);
}

ScopedCurrentChannel::~ScopedCurrentChannel() {
  SetCurrentChannel(original_channel_);
}

}  // namespace extensions

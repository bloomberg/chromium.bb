// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_FEATURES_FEATURE_CHANNEL_H_
#define CHROME_COMMON_EXTENSIONS_FEATURES_FEATURE_CHANNEL_H_

#include "chrome/common/chrome_version_info.h"

namespace extensions {

// Gets the current channel as seen by the Feature system.
chrome::VersionInfo::Channel GetCurrentChannel();

// Sets the current channel as seen by the Feature system. In the browser
// process this should be chrome::VersionInfo::GetChannel(), and in the
// renderer this will need to come from an IPC.
void SetCurrentChannel(chrome::VersionInfo::Channel channel);

// Gets the default channel as seen by the Feature system.
chrome::VersionInfo::Channel GetDefaultChannel();

// Scoped channel setter. Use for tests.
class ScopedCurrentChannel {
 public:
  explicit ScopedCurrentChannel(chrome::VersionInfo::Channel channel)
      : original_channel_(chrome::VersionInfo::CHANNEL_UNKNOWN) {
    original_channel_ = GetCurrentChannel();
    SetCurrentChannel(channel);
  }

  ~ScopedCurrentChannel() {
    SetCurrentChannel(original_channel_);
  }

 private:
  chrome::VersionInfo::Channel original_channel_;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_FEATURES_FEATURE_CHANNEL_H_

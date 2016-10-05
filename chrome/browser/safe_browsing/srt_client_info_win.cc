// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/srt_client_info_win.h"

#include "base/logging.h"
#include "chrome/common/channel_info.h"
#include "components/version_info/version_info.h"

namespace safe_browsing {

const char kChromeVersionSwitch[] = "chrome-version";
const char kChromeChannelSwitch[] = "chrome-channel";
const char kEnableCrashReporting[] = "enable-crash-reporting";

int ChannelAsInt() {
  switch (chrome::GetChannel()) {
    case version_info::Channel::UNKNOWN:
      return 0;
    case version_info::Channel::CANARY:
      return 1;
    case version_info::Channel::DEV:
      return 2;
    case version_info::Channel::BETA:
      return 3;
    case version_info::Channel::STABLE:
      return 4;
  }
  NOTREACHED();
  return 0;
}

}  // namespace safe_browsing

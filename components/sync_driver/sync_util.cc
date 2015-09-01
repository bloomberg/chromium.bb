// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/sync_util.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "components/sync_driver/sync_driver_switches.h"
#include "url/gurl.h"

namespace internal {
const char* kSyncServerUrl = "https://clients4.google.com/chrome-sync";

const char* kSyncDevServerUrl = "https://clients4.google.com/chrome-sync/dev";
}  // namespace internal

GURL GetSyncServiceURL(const base::CommandLine& command_line,
                       version_info::Channel channel) {
  // By default, dev, canary, and unbranded Chromium users will go to the
  // development servers. Development servers have more features than standard
  // sync servers. Users with officially-branded Chrome stable and beta builds
  // will go to the standard sync servers.
  GURL result(internal::kSyncDevServerUrl);

  if (channel == version_info::Channel::STABLE ||
      channel == version_info::Channel::BETA) {
    result = GURL(internal::kSyncServerUrl);
  }

  // Override the sync server URL from the command-line, if sync server
  // command-line argument exists.
  if (command_line.HasSwitch(switches::kSyncServiceURL)) {
    std::string value(
        command_line.GetSwitchValueASCII(switches::kSyncServiceURL));
    if (!value.empty()) {
      GURL custom_sync_url(value);
      if (custom_sync_url.is_valid()) {
        result = custom_sync_url;
      } else {
        LOG(WARNING) << "The following sync URL specified at the command-line "
                     << "is invalid: " << value;
      }
    }
  }
  return result;
}

std::string MakeDesktopUserAgentForSync(version_info::Channel channel) {
  std::string system = "";
#if defined(OS_WIN)
  system = "WIN ";
#elif defined(OS_LINUX)
  system = "LINUX ";
#elif defined(OS_FREEBSD)
  system = "FREEBSD ";
#elif defined(OS_OPENBSD)
  system = "OPENBSD ";
#elif defined(OS_MACOSX)
  system = "MAC ";
#endif
  return MakeUserAgentForSync(system, channel);
}

std::string MakeUserAgentForSync(const std::string& system,
                                 version_info::Channel channel) {
  std::string user_agent;
  user_agent = "Chrome ";
  user_agent += system;
  user_agent += version_info::GetVersionNumber();
  user_agent += " (" + version_info::GetLastChange() + ")";
  if (!version_info::IsOfficialBuild()) {
    user_agent += "-devel";
  } else {
    user_agent += " channel(" + version_info::GetChannelString(channel) + ")";
  }
  return user_agent;
}

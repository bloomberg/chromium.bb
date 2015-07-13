// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SYNC_UTIL_H_
#define CHROME_COMMON_SYNC_UTIL_H_

#include <string>

class GURL;

namespace base {
class CommandLine;
}

namespace chrome {
class VersionInfo;
}

namespace internal {
// Default sync server URL. Visible for testing.
extern const char* kSyncServerUrl;

// Sync server URL for dev channel users. Visible for testing.
extern const char* kSyncDevServerUrl;
}

GURL GetSyncServiceURL(const base::CommandLine& command_line);

// Helper to construct a user agent string (ASCII) suitable for use by
// the syncapi for any HTTP communication. This string is used by the sync
// backend for classifying client types when calculating statistics.
std::string MakeDesktopUserAgentForSync(
    const chrome::VersionInfo& version_info);
std::string MakeUserAgentForSync(const chrome::VersionInfo& version_info,
                                 const std::string& system);

#endif  // CHROME_COMMON_SYNC_UTIL_H_

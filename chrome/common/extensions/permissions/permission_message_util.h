// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_PERMISSIONS_PERMISSION_MESSAGE_UTIL_H_
#define CHROME_COMMON_EXTENSIONS_PERMISSIONS_PERMISSION_MESSAGE_UTIL_H_

#include <set>
#include <string>

namespace extensions {
class PermissionMessage;
class PermissionSet;
class URLPatternSet;
}

namespace permission_message_util {

// Creates the corresponding permission message for a list of hosts.
// The messages change depending on what hosts are present.
extensions::PermissionMessage CreateFromHostList(
    const std::set<std::string>& hosts);

std::set<std::string> GetDistinctHosts(
    const extensions::URLPatternSet& host_patterns,
    bool include_rcd,
    bool exclude_file_scheme);

}  // namespace permission_message_util

#endif  // CHROME_COMMON_EXTENSIONS_PERMISSIONS_PERMISSION_MESSAGE_UTIL_H_

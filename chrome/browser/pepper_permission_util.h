// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PEPPER_PERMISSION_UTIL_H_
#define CHROME_BROWSER_PEPPER_PERMISSION_UTIL_H_

#include <set>
#include <string>

class GURL;
class Profile;

namespace chrome {

// Returns true if the extension or it's shared module is whitelisted, or
// appears in command_line_switch.
bool IsExtensionOrSharedModuleWhitelisted(
    Profile* profile,
    const GURL& url,
    const std::set<std::string>& whitelist,
    const char* command_line_switch);

}  // namespace chrome

#endif  // CHROME_BROWSER_PEPPER_PERMISSION_UTIL_H_

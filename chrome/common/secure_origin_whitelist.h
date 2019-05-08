// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SECURE_ORIGIN_WHITELIST_H_
#define CHROME_COMMON_SECURE_ORIGIN_WHITELIST_H_

#include <set>
#include <string>

class PrefRegistrySimple;

namespace secure_origin_whitelist {

// Returns a whitelist of schemes that should bypass the Is Privileged Context
// check. See http://www.w3.org/TR/powerful-features/#settings-privileged.
std::set<std::string> GetSchemesBypassingSecureContextCheck();

// Register preferences for Secure Origin Whitelists.
//
// Note: the preferences need to be registered both for LocalState and for
// ProfilePrefs - this way LocalState will reflect the correct value (from the
// primary profile) that can be picked up by SecureOriginPrefsObserver.
void RegisterPrefs(PrefRegistrySimple*);

}  // namespace secure_origin_whitelist

#endif  // CHROME_COMMON_SECURE_ORIGIN_WHITELIST_H_

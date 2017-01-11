// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SECURE_ORIGIN_WHITELIST_H_
#define CHROME_COMMON_SECURE_ORIGIN_WHITELIST_H_

#include <set>
#include <string>
#include <vector>

#include "url/gurl.h"

// Return a whitelist of origins that need to be considered trustworthy.
// The whitelist is given by kUnsafelyTreatInsecureOriginAsSecure
// command-line option. See
// https://www.w3.org/TR/powerful-features/#is-origin-trustworthy.
std::vector<GURL> GetSecureOriginWhitelist();

// Returns a whitelist of schemes that should bypass the Is Privileged Context
// check. See http://www.w3.org/TR/powerful-features/#settings-privileged.
std::set<std::string> GetSchemesBypassingSecureContextCheckWhitelist();

#endif  // CHROME_COMMON_SECURE_ORIGIN_WHITELIST_H_

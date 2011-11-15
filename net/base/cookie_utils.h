// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_COOKIE_UTILS_H_
#define NET_BASE_COOKIE_UTILS_H_
#pragma once

#include <string>

class GURL;

namespace net {
namespace cookie_utils {

// Returns the effective TLD+1 for a given host. This only makes sense for http
// and https schemes. For other schemes, the host will be returned unchanged
// (minus any leading period).
std::string GetEffectiveDomain(const std::string& scheme,
                               const std::string& host);

// Determine the actual cookie domain based on the domain string passed
// (if any) and the URL from which the cookie came.
// On success returns true, and sets cookie_domain to either a
//   -host cookie domain (ex: "google.com")
//   -domain cookie domain (ex: ".google.com")
bool GetCookieDomainWithString(const GURL& url,
                               const std::string& domain_string,
                               std::string* result);

// Returns true if a domain string represents a host-only cookie,
// i.e. it doesn't begin with a leading '.' character.
bool DomainIsHostOnly(const std::string& domain_string);

}  // namspace cookie_utils
}  // namespace net

#endif  // NET_BASE_COOKIE_UTILS_H_

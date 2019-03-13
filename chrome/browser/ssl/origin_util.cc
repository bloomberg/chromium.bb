// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/origin_util.h"

#include <string>
#include <vector>

#include "base/strings/pattern.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/origin_util.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "url/gurl.h"

namespace {

// Returns a vector containing all origins and patterns allowlisted as "Secure"
// by the OverrideSecurityRestrictionsOnInsecureOrigin policy.
std::vector<std::string> GetSecureOriginsAndPatterns(PrefService* prefs) {
  if (prefs->HasPrefPath(prefs::kUnsafelyTreatInsecureOriginAsSecure)) {
    return network::ParseSecureOriginAllowlist(
        prefs->GetString(prefs::kUnsafelyTreatInsecureOriginAsSecure));
  }
  return std::vector<std::string>();
}

// Returns true if |origin| matches an origin or pattern in the allowlist from
// the OverrideSecurityRestrictionsOnInsecureOrigin policy.
bool IsPolicyWhitelistedAsSecureOrigin(const url::Origin& origin,
                                       PrefService* prefs) {
  std::vector<std::string> allowlist = GetSecureOriginsAndPatterns(prefs);
  return network::IsAllowlistedAsSecureOrigin(origin, allowlist);
}

}  // namespace

bool IsOriginSecure(const GURL& url, PrefService* prefs) {
  // content::IsOriginSecure() also checks for the
  // "--unsafely-treat-insecure-origin-as-secure" command line flag.
  if (content::IsOriginSecure(url)) {
    return true;
  }
  return IsPolicyWhitelistedAsSecureOrigin(url::Origin::Create(url), prefs);
}

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/users/affiliation.h"

#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace chromeos {

bool HaveCommonElement(const std::set<std::string>& set1,
                       const std::set<std::string>& set2) {
  std::set<std::string>::const_iterator it1 = set1.begin();
  std::set<std::string>::const_iterator it2 = set2.begin();

  while (it1 != set1.end() && it2 != set2.end()) {
    if (*it1 == *it2)
      return true;
    if (*it1 < *it2) {
      ++it1;
    } else {
      ++it2;
    }
  }
  return false;
}

bool IsUserAffiliated(const AffiliationIDSet& user_affiliation_ids,
                      const AffiliationIDSet& device_affiliation_ids,
                      const std::string& email,
                      const std::string& enterprise_domain) {
  // An empty username means incognito user in case of ChromiumOS and
  // no logged-in user in case of Chromium (SigninService). Many tests use
  // nonsense email addresses (e.g. 'test') so treat those as non-enterprise
  // users.
  if (email.empty() || email.find('@') == std::string::npos) {
    return false;
  }

  if (policy::IsDeviceLocalAccountUser(email, NULL)) {
    return true;
  }

  if (!device_affiliation_ids.empty() && !user_affiliation_ids.empty()) {
    return HaveCommonElement(user_affiliation_ids, device_affiliation_ids);
  }

  // TODO(peletskyi): Remove the following backwards compatibility part.
  if (gaia::ExtractDomainName(gaia::CanonicalizeEmail(email)) ==
      enterprise_domain) {
    return true;
  }

  return false;
}

}  // namespace chromeos

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/users/affiliation.h"

#include "base/command_line.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "components/policy/core/common/policy_switches.h"
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
                      const std::string& email) {
  // An empty username means incognito user in case of Chrome OS and no
  // logged-in user in case of Chrome (SigninService). Many tests use nonsense
  // email addresses (e.g. 'test') so treat those as non-enterprise users.
  if (email.empty() || email.find('@') == std::string::npos) {
    return false;
  }

  if (policy::IsDeviceLocalAccountUser(email, NULL)) {
    return true;
  }

  // Not all test servers correctly support affiliation ids so far, so
  // this is a work-around.
  // TODO(antrim): remove this once all test servers support affiliation ids.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(policy::switches::kUserAlwaysAffiliated)) {
    return true;
  }

  if (!device_affiliation_ids.empty() && !user_affiliation_ids.empty()) {
    return HaveCommonElement(user_affiliation_ids, device_affiliation_ids);
  }

  return false;
}

}  // namespace chromeos

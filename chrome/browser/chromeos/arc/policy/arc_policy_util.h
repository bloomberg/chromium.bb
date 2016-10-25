// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_POLICY_ARC_POLICY_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_ARC_POLICY_ARC_POLICY_UTIL_H_

class Profile;

namespace arc {
namespace policy_util {

// Returns true if the account is managed. Otherwise false.
bool IsAccountManaged(Profile* profile);

// Returns true if ARC is disabled by --enterprise-diable-arc flag.
bool IsArcDisabledForEnterprise();

}  // namespace policy_util
}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_POLICY_ARC_POLICY_UTIL_H_

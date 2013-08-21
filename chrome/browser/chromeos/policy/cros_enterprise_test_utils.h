// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_CROS_ENTERPRISE_TEST_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_CROS_ENTERPRISE_TEST_UTILS_H_

#include <string>

namespace base {
class FilePath;
}

namespace policy {

namespace test_utils {

// Marks the device as enterprise-owned.
void MarkAsEnterpriseOwned(const std::string& username,
                           const base::FilePath& temp_dir);

}  // namespace test_utils

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_CROS_ENTERPRISE_TEST_UTILS_H_

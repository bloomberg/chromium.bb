// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_CONFIGURATION_KEYS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_CONFIGURATION_KEYS_H_

namespace chromeos {
namespace configuration {
// Configuration keys that are used to automate OOBE screens go here.
// Please keep keys grouped by screens and ordered according to OOBE flow.

extern const char kWelcomeNext[];

extern const char kNetworkSelectGUID[];

extern const char kEULASendUsageStatistics[];

extern const char kEULAAutoAccept[];

extern const char kUpdateSkipUpdate[];

extern const char kWizardAutoEnroll[];

}  // namespace configuration
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_CONFIGURATION_KEYS_H_

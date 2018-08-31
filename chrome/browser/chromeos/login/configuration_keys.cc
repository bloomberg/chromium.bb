// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/configuration_keys.h"

namespace chromeos {
namespace configuration {

// Configuration keys that are used to automate OOBE screens go here.
// Please keep keys grouped by screens and ordered according to OOBE flow.
// All keys should be listed here, even if they are used in JS code only.
// These keys are used in chrome/browser/resources/chromeos/login/oobe_types.js

// == Welcome screen:

// Boolean value indicating if "Next" button on welcome screen is pressed
// automatically.
const char kWelcomeNext[] = "welcomeNext";

// == Network screen:

// String value specifying GUID of the network that would be automatically
// selected.
const char kNetworkSelectGUID[] = "networkSelectGuid";

// == EULA screen:

// Boolean value indicating if device should send usage statistics.
const char kEULASendUsageStatistics[] = "eulaSendStatistics";

// Boolean value indicating if the EULA is automatically accepted.
const char kEULAAutoAccept[] = "eulaAutoAccept";

// == Update screen:

// Boolean value, indicating that update check should be skipped entirely
// (it might be required for future version pinning)
const char kUpdateSkipUpdate[] = "updateSkip";

// == Wizard controller:

// Boolean value, controls if WizardController should automatically start
// enrollment at appropriate moment.
const char kWizardAutoEnroll[] = "wizardAutoEnroll";

}  // namespace configuration
}  // namespace chromeos

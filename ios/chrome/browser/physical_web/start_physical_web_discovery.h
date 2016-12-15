// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PHYSICAL_WEB_START_PHYSICAL_WEB_DISCOVERY_H_
#define IOS_CHROME_BROWSER_PHYSICAL_WEB_START_PHYSICAL_WEB_DISCOVERY_H_

#include "components/prefs/pref_service.h"

namespace ios {
class ChromeBrowserState;
}

// Checks the environment and starts Physical Web discovery if the required
// conditions are met.
void StartPhysicalWebDiscovery(PrefService* pref_service,
                               ios::ChromeBrowserState* browser_state);

#endif  // IOS_CHROME_BROWSER_PHYSICAL_WEB_START_PHYSICAL_WEB_DISCOVERY_H_

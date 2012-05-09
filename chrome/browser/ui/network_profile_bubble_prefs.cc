// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/network_profile_bubble_prefs.h"

#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"

namespace browser {

const int kMaxWarnings = 2;

void RegisterNetworkProfileBubblePrefs(PrefService* prefs) {
  prefs->RegisterIntegerPref(prefs::kNetworkProfileWarningsLeft,
                             kMaxWarnings,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterInt64Pref(prefs::kNetworkProfileLastWarningTime,
                           0,
                           PrefService::UNSYNCABLE_PREF);
}

}  // namespace browser

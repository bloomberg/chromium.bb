// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/invert_bubble_prefs.h"

#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"

namespace chrome {

void RegisterInvertBubbleUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kInvertNotificationShown,
                             false,
                             PrefService::UNSYNCABLE_PREF);
}

}  // namespace chrome

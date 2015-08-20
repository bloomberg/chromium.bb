// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/prefs/browser_prefs.h"

#include "base/prefs/pref_service.h"
#include "ios/chrome/browser/pref_names.h"

// This method should be periodically pruned of year+ old migrations.
void MigrateObsoleteIOSProfilePrefs(PrefService* prefs) {
  // Added 08/2015.
  prefs->ClearPref(::prefs::kSigninSharedAuthenticationUserId);
}

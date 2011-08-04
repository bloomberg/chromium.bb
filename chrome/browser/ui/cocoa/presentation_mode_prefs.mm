// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/presentation_mode_prefs.h"

#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"

// static
void PresentationModePrefs::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kPresentationModeEnabled,
                             false,
                             PrefService::UNSYNCABLE_PREF);
}

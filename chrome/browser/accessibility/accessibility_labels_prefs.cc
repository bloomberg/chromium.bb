// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/accessibility_labels_prefs.h"

#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace accessibility_prefs {

void RegisterAccessibilityLabelsProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kAccessibilityImageLabelsEnabled, false);
}

}  // namespace accessibility_prefs

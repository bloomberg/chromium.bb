// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_LABELS_PREFS_H_
#define CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_LABELS_PREFS_H_

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace accessibility_prefs {

// The accessibility labels preference should be registered on a user profile.
void RegisterAccessibilityLabelsProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry);

}  // namespace accessibility_prefs

#endif  // CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_LABELS_PREFS_H_

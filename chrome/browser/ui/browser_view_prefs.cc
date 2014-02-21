// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_view_prefs.h"

#include "base/prefs/pref_registry_simple.h"
#include "chrome/common/pref_names.h"
#include "components/user_prefs/pref_registry_syncable.h"

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "ui/base/x/x11_util.h"
#endif

namespace {

// How long do we wait before we consider a window hung (in ms).
const int kDefaultPluginMessageResponseTimeout = 25000;

// How frequently we check for hung plugin windows.
const int kDefaultHungPluginDetectFrequency = 2000;

}  // namespace

namespace chrome {

void RegisterBrowserViewLocalPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kPluginMessageResponseTimeout,
                                kDefaultPluginMessageResponseTimeout);
  registry->RegisterIntegerPref(prefs::kHungPluginDetectFrequency,
                                kDefaultHungPluginDetectFrequency);
}

void RegisterBrowserViewProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  bool custom_frame_default = false;
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  custom_frame_default = ui::GetCustomFramePrefDefault();
#endif

  registry->RegisterBooleanPref(
      prefs::kUseCustomChromeFrame,
      custom_frame_default,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

}  // namespace chrome

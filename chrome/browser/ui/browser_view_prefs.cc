// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_view_prefs.h"

#include "base/prefs/pref_registry_simple.h"
#include "chrome/common/pref_names.h"

namespace {

// How long do we wait before we consider a window hung (in ms).
const int kDefaultPluginMessageResponseTimeout = 25000;

// How frequently we check for hung plugin windows.
const int kDefaultHungPluginDetectFrequency = 2000;

}  // namespace

namespace chrome {

void RegisterBrowserViewPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kPluginMessageResponseTimeout,
                                kDefaultPluginMessageResponseTimeout);
  registry->RegisterIntegerPref(prefs::kHungPluginDetectFrequency,
                                kDefaultHungPluginDetectFrequency);
}

}  // namespace chrome

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/autolaunch_prompt.h"

#include "components/pref_registry/pref_registry_syncable.h"

namespace chrome {

bool ShowAutolaunchPrompt(Browser* browser) {
  // Autolaunch is only implemented on Windows right now.
  return false;
}

void RegisterAutolaunchUserPrefs(user_prefs::PrefRegistrySyncable* registry) {
  // Autolaunch is only implemented on Windows right now.
}

}  // namespace chrome

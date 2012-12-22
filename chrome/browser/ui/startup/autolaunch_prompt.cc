// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/autolaunch_prompt.h"

namespace chrome {

bool ShowAutolaunchPrompt(Browser* browser) {
  // Autolaunch is only implemented on Windows right now.
  return false;
}

void RegisterAutolaunchUserPrefs(PrefServiceSyncable* prefs) {
  // Autolaunch is only implemented on Windows right now.
}

}  // namespace chrome

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autolaunch_prompt.h"

namespace browser {

bool ShowAutolaunchPrompt(Profile* profile) {
  // Autolaunch is only implemented on Windows right now.
  return false;
}

void RegisterAutolaunchPrefs(PrefService* prefs) {
  // Autolaunch is only implemented on Windows right now.
}

}  // namespace browser

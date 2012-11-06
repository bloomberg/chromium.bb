// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Stub implementation of web_intents_util.h to be included in builds
// where ENABLE_WEB_INTENTS is not defined.

#include "chrome/browser/intents/web_intents_util.h"

namespace web_intents {

const char kActionView[] = "http://webintents.org/view";

bool IsWebIntentsEnabled(PrefService* prefs) {
  return false;
}

bool IsWebIntentsEnabledForProfile(Profile* profile) {
  return false;
}

Browser* GetBrowserForBackgroundWebIntentDelivery(Profile* profile) {
  return NULL;
}

}  // namespace web_intents

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/preferences/command_line_pref_store_android.h"

#include "blimp/client/public/blimp_client_context.h"
#include "components/prefs/command_line_pref_store.h"

namespace android {

void ApplyBlimpSwitches(CommandLinePrefStore* store) {
  blimp::client::BlimpClientContext::ApplyBlimpSwitches(store);
}

}  // namespace android

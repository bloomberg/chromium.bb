// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PREFERENCES_COMMAND_LINE_PREF_STORE_ANDROID_H_
#define CHROME_BROWSER_ANDROID_PREFERENCES_COMMAND_LINE_PREF_STORE_ANDROID_H_

class CommandLinePrefStore;

namespace android {

// Delegating handling blimp command line switches to blimp.
void ApplyBlimpSwitches(CommandLinePrefStore* store);

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_PREFERENCES_COMMAND_LINE_PREF_STORE_ANDROID_H_

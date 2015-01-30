// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PREFERENCES_AUTOFILL_AUTOFILL_PROFILE_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_PREFERENCES_AUTOFILL_AUTOFILL_PROFILE_BRIDGE_H_

#include <jni.h>

namespace autofill {

// Registers native methods.
bool RegisterAutofillProfileBridge(JNIEnv* env);

} // namespace autofill

#endif  // CHROME_BROWSER_ANDROID_PREFERENCES_AUTOFILL_AUTOFILL_PROFILE_BRIDGE_H_

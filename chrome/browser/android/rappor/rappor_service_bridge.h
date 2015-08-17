// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_RAPPOR_RAPPOR_SERVICE_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_RAPPOR_RAPPOR_SERVICE_BRIDGE_H_

#include <jni.h>
#include <string>

namespace rappor {

bool RegisterRapporServiceBridge(JNIEnv* env);

}  // namespace rappor

#endif  // CHROME_BROWSER_ANDROID_RAPPOR_RAPPOR_SERVICE_BRIDGE_H_

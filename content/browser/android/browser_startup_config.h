// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_STARTUP_CONFIGURATION_H_
#define CONTENT_BROWSER_BROWSER_STARTUP_CONFIGURATION_H_

#include <jni.h>

namespace content {

bool BrowserMayStartAsynchronously();
void BrowserStartupComplete(int result);

bool RegisterBrowserStartupConfig(JNIEnv* env);

}  // namespace content
#endif  // CONTENT_BROWSER_BROWSER_STARTUP_CONFIGURATION_H_

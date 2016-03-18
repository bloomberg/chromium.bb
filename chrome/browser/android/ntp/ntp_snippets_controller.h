// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_NTP_NTP_SNIPPETS_CONTROLLER_H_
#define CHROME_BROWSER_ANDROID_NTP_NTP_SNIPPETS_CONTROLLER_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"

// The C++ counterpart to SnippetsController.java. Enables Java code to fetch
// snippets from the server
class NTPSnippetsController {
 public:
  static bool Register(JNIEnv* env);

  DISALLOW_COPY_AND_ASSIGN(NTPSnippetsController);
};

#endif  // CHROME_BROWSER_ANDROID_NTP_NTP_SNIPPETS_CONTROLLER_H_

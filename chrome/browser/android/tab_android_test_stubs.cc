// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains stubs for some Chrome for Android specific code that is
// needed to compile some tests.

#include "chrome/browser/android/tab_android.h"

// static
TabAndroid* TabAndroid::FromWebContents(content::WebContents* web_contents) {
  return NULL;
}

// static
TabAndroid* TabAndroid::GetNativeTab(JNIEnv* env, jobject obj) {
  return NULL;
}

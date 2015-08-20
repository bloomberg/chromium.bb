// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SHORTCUT_HELPER_H_
#define CHROME_BROWSER_ANDROID_SHORTCUT_HELPER_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "chrome/browser/android/shortcut_info.h"
#include "third_party/skia/include/core/SkBitmap.h"

// ShortcutHelper is the C++ counterpart of org.chromium.chrome.browser's
// ShortcutHelper in Java. The object is owned by the Java object. It is created
// from there via a JNI (Initialize) call and MUST BE DESTROYED via Destroy().
class ShortcutHelper {
 public:
  // Registers JNI hooks.
  static bool RegisterShortcutHelper(JNIEnv* env);

  // Adds a shortcut to the launcher using a SkBitmap.
  // Must be called on the IO thread.
  static void AddShortcutInBackgroundWithSkBitmap(const ShortcutInfo& info,
                                                  const SkBitmap& icon_bitmap);
 private:
  ShortcutHelper();

  DISALLOW_COPY_AND_ASSIGN(ShortcutHelper);
};

#endif  // CHROME_BROWSER_ANDROID_SHORTCUT_HELPER_H_

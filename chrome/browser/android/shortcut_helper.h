// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SHORTCUT_HELPER_H_
#define CHROME_BROWSER_ANDROID_SHORTCUT_HELPER_H_

#include "base/android/jni_helper.h"
#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "chrome/browser/android/tab_android.h"

namespace chrome {
struct FaviconBitmapResult;
}  // namespace chrome

namespace content {
class WebContents;
}  // namespace content

class GURL;

// Adds a shortcut to the current URL to the Android home screen.
class ShortcutHelper {
 public:
  // Adds a shortcut to the current URL to the Android home screen, firing
  // background tasks to pull all the data required.
  static void AddShortcut(content::WebContents* web_contents);

  // Registers JNI hooks.
  static bool RegisterShortcutHelper(JNIEnv* env);

  // Adds a shortcut to the launcher.
  static void FinishAddingShortcut(
      const GURL& url,
      const base::string16& title,
      bool is_webapp_capable,
      const chrome::FaviconBitmapResult& bitmap_result);

 private:
  // Adds a shortcut to the launcher in the background.
  static void FinishAddingShortcutInBackground(
      const GURL& url,
      const base::string16& title,
      bool is_webapp_capable,
      const chrome::FaviconBitmapResult& bitmap_result);

  DISALLOW_COPY_AND_ASSIGN(ShortcutHelper);
};

#endif  // CHROME_BROWSER_ANDROID_SHORTCUT_HELPER_H_

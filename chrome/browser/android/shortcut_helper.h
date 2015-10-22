// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SHORTCUT_HELPER_H_
#define CHROME_BROWSER_ANDROID_SHORTCUT_HELPER_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "chrome/browser/android/shortcut_info.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace content {
class WebContents;
}  // namespace content

// ShortcutHelper is the C++ counterpart of org.chromium.chrome.browser's
// ShortcutHelper in Java.
class ShortcutHelper {
 public:
  // Registers JNI hooks.
  static bool RegisterShortcutHelper(JNIEnv* env);

  // Adds a shortcut to the launcher using a SkBitmap.
  // Must be called on the IO thread.
  static void AddShortcutInBackgroundWithSkBitmap(const ShortcutInfo& info,
                                                  const std::string& webapp_id,
                                                  const SkBitmap& icon_bitmap);

  // Returns the ideal size for an icon representing a web app.
  static int GetIdealHomescreenIconSizeInDp();

  // Returns the minimum size for an icon representing a web app.
  static int GetMinimumHomescreenIconSizeInDp();

  // Returns the ideal size for an image displayed on a web app's splash
  // screen.
  static int GetIdealSplashImageSizeInDp();

  // Returns the minimum size for an image displayed on a web app's splash
  // screen.
  static int GetMinimumSplashImageSizeInDp();

  // Fetches the splash screen image and stores it inside the WebappDataStorage
  // of the webapp.
  static void FetchSplashScreenImage(content::WebContents* web_contents,
                                     const GURL& image_url,
                                     const int ideal_splash_image_size_in_dp,
                                     const int minimum_splash_image_size_in_dp,
                                     const std::string& webapp_id);

  // Stores the data of the webapp which is not placed inside the shortcut.
  static void StoreWebappData(const std::string& webapp_id,
                              const SkBitmap& splash_image);

  // Modify the given icon to matche the launcher requirements, then returns the
  // new icon. It might generate an entirely new icon, in which case,
  // |is_generated| will be set to |true|.
  static SkBitmap FinalizeLauncherIcon(const SkBitmap& icon,
                                       const GURL& url,
                                       bool* is_generated);

 private:
  ShortcutHelper() = delete;
  ~ShortcutHelper() = delete;

  DISALLOW_COPY_AND_ASSIGN(ShortcutHelper);
};

#endif  // CHROME_BROWSER_ANDROID_SHORTCUT_HELPER_H_

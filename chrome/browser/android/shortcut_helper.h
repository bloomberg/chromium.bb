// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SHORTCUT_HELPER_H_
#define CHROME_BROWSER_ANDROID_SHORTCUT_HELPER_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/callback_forward.h"
#include "base/macros.h"
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

  // Adds a shortcut to the launcher using a SkBitmap. If the shortcut is for
  // a standalone-capable site, |splash_image_callback| will be invoked once the
  // Java-side operation has completed. This is necessary as Java will
  // asynchronously create and populate a WebappDataStorage object for
  // standalone-capable sites. This must exist before the splash image can be
  // stored.
  // Must not be called on the UI thread.
  static void AddShortcutInBackgroundWithSkBitmap(
      const ShortcutInfo& info,
      const std::string& webapp_id,
      const SkBitmap& icon_bitmap,
      const base::Closure& splash_image_callback);

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
  // of the webapp. The WebappDataStorage object *must* have been previously
  // created by |AddShortcutInBackgroundWithSkBitmap|; this method should be
  // passed as a closure to that method.
  static void FetchSplashScreenImage(content::WebContents* web_contents,
                                     const GURL& image_url,
                                     const int ideal_splash_image_size_in_dp,
                                     const int minimum_splash_image_size_in_dp,
                                     const std::string& webapp_id);

  // Stores the webapp splash screen in the WebappDataStorage associated with
  // |webapp_id|.
  static void StoreWebappSplashImage(const std::string& webapp_id,
                                     const SkBitmap& splash_image);

  // Returns the given icon, modified to match the launcher requirements.
  // This method may generate an entirely new icon; if this is the case,
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

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
#include "chrome/browser/android/webapk/webapk_installer.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

// ShortcutHelper is the C++ counterpart of org.chromium.chrome.browser's
// ShortcutHelper in Java.
class ShortcutHelper {
 public:
  // Registers JNI hooks.
  static bool RegisterShortcutHelper(JNIEnv* env);

  // Adds a shortcut to the launcher using a SkBitmap. The type of shortcut
  // added depends on the properties in |info|. Calls one of
  // InstallWebApkInBackgroundWithSkBitmap, AddWebappInBackgroundWithSkBitmap,
  // or AddShortcutInBackgroundWithSkBitmap.
  static void AddToLauncherWithSkBitmap(
      content::BrowserContext* browser_context,
      const ShortcutInfo& info,
      const std::string& webapp_id,
      const SkBitmap& icon_bitmap,
      const base::Closure& splash_image_callback);

  // Installs WebAPK and adds shortcut to the launcher.
  static void InstallWebApkWithSkBitmap(
      content::BrowserContext* browser_context,
      const ShortcutInfo& info,
      const SkBitmap& icon_bitmap,
      const WebApkInstaller::FinishCallback& callback);

  // Adds a shortcut which opens in a fullscreen window to the launcher.
  // |splash_image_callback| will be invoked once the Java-side operation has
  // completed. This is necessary as Java will asynchronously create and
  // populate a WebappDataStorage object for standalone-capable sites. This must
  // exist before the splash image can be stored.
  static void AddWebappWithSkBitmap(
      const ShortcutInfo& info,
      const std::string& webapp_id,
      const SkBitmap& icon_bitmap,
      const base::Closure& splash_image_callback);

  // Adds a shortcut which opens in a browser tab to the launcher.
  static void AddShortcutWithSkBitmap(
      const ShortcutInfo& info,
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
  // Must be called on a background worker thread.
  static SkBitmap FinalizeLauncherIconInBackground(const SkBitmap& icon,
                                                   const GURL& url,
                                                   bool* is_generated);

  // Returns the package name of the WebAPK if WebAPKs are enabled and there is
  // an installed WebAPK which can handle |url|. Returns empty string otherwise.
  static std::string QueryWebApkPackage(const GURL& url);

  // Returns true if WebAPKs are enabled and there is an installed WebAPK which
  // can handle |url|.
  static bool IsWebApkInstalled(const GURL& url);

  // Generates a scope URL based on the passed in |url|. It should be used
  // when the Web Manifest does not specify a scope URL.
  static GURL GetScopeFromURL(const GURL& url);

 private:
  ShortcutHelper() = delete;
  ~ShortcutHelper() = delete;

  DISALLOW_COPY_AND_ASSIGN(ShortcutHelper);
};

#endif  // CHROME_BROWSER_ANDROID_SHORTCUT_HELPER_H_

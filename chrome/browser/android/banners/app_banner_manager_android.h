// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_BANNERS_APP_BANNER_MANAGER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_BANNERS_APP_BANNER_MANAGER_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/android/banners/app_banner_data_fetcher_android.h"
#include "chrome/browser/banners/app_banner_debug_log.h"
#include "chrome/browser/banners/app_banner_manager.h"

namespace banners {
class AppBannerDataFetcherAndroid;

// Extends the AppBannerManager to support native Android apps.
// TODO(dfalcantara): Flip it so the C++ AppBannerManagerAndroid owns the
// Java AppBannerManager, move ownership of the AppBannerManagerAndroid to
// the TabAndroid class, then move functions for retriving info from Java to
// the AppBannerDataFetcherAndroid.
class AppBannerManagerAndroid : public AppBannerManager {
 public:
  // Registers native methods.
  static bool Register(JNIEnv* env);

  AppBannerManagerAndroid(JNIEnv* env, jobject obj);
  ~AppBannerManagerAndroid() override;

  // Destroys the AppBannerManagerAndroid.
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  // Observes a new WebContents, if necessary.
  void ReplaceWebContents(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jweb_contents);

  // Return whether a BitmapFetcher is active.
  bool IsFetcherActive(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& jobj);

  // Called when the Java-side has retrieved information for the app.
  // Returns |false| if an icon fetch couldn't be kicked off.
  bool OnAppDetailsRetrieved(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& japp_data,
      const base::android::JavaParamRef<jstring>& japp_title,
      const base::android::JavaParamRef<jstring>& japp_package,
      const base::android::JavaParamRef<jstring>& jicon_url);

  // Requests the app banner. This method is called from the DevTools.
  void RequestAppBanner(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& jobj);

 protected:
  AppBannerDataFetcher* CreateAppBannerDataFetcher(
      base::WeakPtr<AppBannerDataFetcher::Delegate> weak_delegate,
      bool is_debug_mode) override;

 private:
  // AppBannerDataFetcher::Delegate overrides.
  bool HandleNonWebApp(const std::string& platform,
                       const GURL& url,
                       const std::string& id,
                       bool is_debug_mode) override;

  bool CheckPlatformAndId(const std::string& platform,
                          const std::string& id,
                          bool is_debug_mode);

  bool CheckFetcherMatchesContents(bool is_debug_mode);

  std::string ExtractQueryValueForName(const GURL& url,
                                       const std::string& name);

  // AppBannerManager on the Java side.
  JavaObjectWeakGlobalRef weak_java_banner_view_manager_;

  DISALLOW_COPY_AND_ASSIGN(AppBannerManagerAndroid);
};  // class AppBannerManagerAndroid

}  // namespace banners

#endif  // CHROME_BROWSER_ANDROID_BANNERS_APP_BANNER_MANAGER_ANDROID_H_

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPPS_ADD_TO_HOMESCREEN_MANAGER_H_
#define CHROME_BROWSER_ANDROID_WEBAPPS_ADD_TO_HOMESCREEN_MANAGER_H_

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/android/webapps/add_to_homescreen_data_fetcher.h"

namespace content {
class WebContents;
}

class SkBitmap;
struct ShortcutInfo;

// AddToHomescreenManager is the C++ counterpart of
// org.chromium.chrome.browser's AddToHomescreenManager in Java. The object
// is owned by the Java object. It is created from there via a JNI
// (InitializeAndStart) call and MUST BE DESTROYED via Destroy().
class AddToHomescreenManager : public AddToHomescreenDataFetcher::Observer {
 public:
  AddToHomescreenManager(JNIEnv* env, jobject obj);

  // Called by the Java counterpart to destroy its native half.
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  // Adds a shortcut to the current URL to the Android home screen.
  void AddShortcut(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj,
                   const base::android::JavaParamRef<jstring>& title);

  // Starts the add-to-homescreen process.
  void Start(content::WebContents* web_contents);

 private:
  ~AddToHomescreenManager() override;

  void RecordAddToHomescreen();

  // AddToHomescreenDataFetcher::Observer:
  void OnUserTitleAvailable(const base::string16& user_title,
                            const GURL& url,
                            bool is_webapk_compatible) override;
  void OnDataAvailable(const ShortcutInfo& info,
                       const SkBitmap& primary_icon,
                       const SkBitmap& badge_icon) override;

  // Points to the Java object.
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  // Whether the site is WebAPK-compatible.
  bool is_webapk_compatible_;

  // Fetches data required to add a shortcut.
  std::unique_ptr<AddToHomescreenDataFetcher> data_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(AddToHomescreenManager);
};

#endif  // CHROME_BROWSER_ANDROID_WEBAPPS_ADD_TO_HOMESCREEN_MANAGER_H_

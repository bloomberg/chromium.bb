// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapps/add_to_homescreen_dialog_helper.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/basictypes.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/shortcut_helper.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/manifest.h"
#include "jni/AddToHomescreenDialogHelper_jni.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/color_analysis.h"
#include "url/gurl.h"

using content::Manifest;

jlong Initialize(JNIEnv* env,
                 const JavaParamRef<jobject>& obj,
                 const JavaParamRef<jobject>& java_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  AddToHomescreenDialogHelper* add_to_homescreen_helper =
      new AddToHomescreenDialogHelper(env, obj, web_contents);
  return reinterpret_cast<intptr_t>(add_to_homescreen_helper);
}

AddToHomescreenDialogHelper::AddToHomescreenDialogHelper(
    JNIEnv* env,
    jobject obj,
    content::WebContents* web_contents)
    : add_shortcut_pending_(false),
      data_fetcher_(new AddToHomescreenDataFetcher(web_contents,
          ShortcutHelper::GetIdealHomescreenIconSizeInDp(),
          ShortcutHelper::GetMinimumHomescreenIconSizeInDp(),
          ShortcutHelper::GetIdealSplashImageSizeInDp(),
          ShortcutHelper::GetMinimumSplashImageSizeInDp(),
          this)) {
  java_ref_.Reset(env, obj);
}

AddToHomescreenDialogHelper::~AddToHomescreenDialogHelper() {
  data_fetcher_->set_weak_observer(nullptr);
  data_fetcher_ = nullptr;
}

void AddToHomescreenDialogHelper::OnUserTitleAvailable(
    const base::string16& user_title) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_user_title =
      base::android::ConvertUTF16ToJavaString(env, user_title);
  Java_AddToHomescreenDialogHelper_onUserTitleAvailable(env,
                                                  java_ref_.obj(),
                                                  j_user_title.obj());
}

void AddToHomescreenDialogHelper::OnDataAvailable(const ShortcutInfo& info,
                                                  const SkBitmap& icon) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_bitmap;
  if (icon.getSize())
    java_bitmap = gfx::ConvertToJavaBitmap(&icon);

  Java_AddToHomescreenDialogHelper_onIconAvailable(env,
                                             java_ref_.obj(),
                                             java_bitmap.obj());

  if (add_shortcut_pending_)
    AddShortcut(info, icon);
}

void AddToHomescreenDialogHelper::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

SkBitmap AddToHomescreenDialogHelper::FinalizeLauncherIcon(
      const SkBitmap& bitmap,
      const GURL& url,
      bool* is_generated) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  return ShortcutHelper::FinalizeLauncherIcon(bitmap, url, is_generated);
}

void AddToHomescreenDialogHelper::AddShortcut(JNIEnv* env,
                                        jobject obj,
                                        jstring j_user_title) {
  add_shortcut_pending_ = true;

  base::string16 user_title =
      base::android::ConvertJavaStringToUTF16(env, j_user_title);
  if (!user_title.empty())
    data_fetcher_->shortcut_info().user_title = user_title;

  if (data_fetcher_->is_ready()) {
    // If the fetcher isn't ready yet, the shortcut will be added when it is
    // via OnDataAvailable();
    AddShortcut(data_fetcher_->shortcut_info(), data_fetcher_->shortcut_icon());
  }
}

void AddToHomescreenDialogHelper::AddShortcut(const ShortcutInfo& info,
                                              const SkBitmap& icon) {
  DCHECK(add_shortcut_pending_);
  if (!add_shortcut_pending_)
    return;
  add_shortcut_pending_ = false;

  RecordAddToHomescreen();

  const std::string& uid = base::GenerateGUID();
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&ShortcutHelper::AddShortcutInBackgroundWithSkBitmap,
                 info,
                 uid,
                 icon));

  data_fetcher_->FetchSplashScreenImage(uid);
}

bool AddToHomescreenDialogHelper::RegisterAddToHomescreenDialogHelper(
      JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void AddToHomescreenDialogHelper::RecordAddToHomescreen() {
  // Record that the shortcut has been added, so no banners will be shown
  // for this app.
  content::WebContents* web_contents = data_fetcher_->web_contents();
  if (!web_contents)
    return;

  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents, web_contents->GetURL(),
      data_fetcher_->shortcut_info().url.spec(),
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_ADD_TO_HOMESCREEN,
      base::Time::Now());
}

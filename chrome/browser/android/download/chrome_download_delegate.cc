// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/download/chrome_download_delegate.h"

#include <jni.h>

#include <string>
#include <type_traits>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "chrome/browser/android/download/android_download_manager_duplicate_infobar_delegate.h"
#include "chrome/browser/android/download/download_controller_base.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/permissions/permission_update_infobar_delegate_android.h"
#include "chrome/common/safe_browsing/file_type_policies.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "jni/ChromeDownloadDelegate_jni.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using content::WebContents;

// Gets the download warning text for the given file name.
static ScopedJavaLocalRef<jstring> GetDownloadWarningText(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jstring>& filename) {
  return base::android::ConvertUTF8ToJavaString(
      env, l10n_util::GetStringFUTF8(
               IDS_PROMPT_DANGEROUS_DOWNLOAD,
               base::android::ConvertJavaStringToUTF16(env, filename)));
}

// static
bool ChromeDownloadDelegate::EnqueueDownloadManagerRequest(
    jobject chrome_download_delegate,
    bool overwrite,
    jobject download_info) {
  JNIEnv* env = base::android::AttachCurrentThread();

  return Java_ChromeDownloadDelegate_enqueueDownloadManagerRequestFromNative(
      env, chrome_download_delegate, overwrite, download_info);
}

// Called when we need to interrupt download and ask user whether to proceed
// as there is already an existing file.
static void LaunchDuplicateDownloadInfoBar(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jobject>& delegate,
    const JavaParamRef<jobject>& tab,
    const JavaParamRef<jobject>& download_info,
    const JavaParamRef<jstring>& jfile_path,
    jboolean is_incognito) {
  TabAndroid* tab_android = TabAndroid::GetNativeTab(env, tab);

  std::string file_path =
      base::android::ConvertJavaStringToUTF8(env, jfile_path);

  chrome::android::AndroidDownloadManagerDuplicateInfoBarDelegate::Create(
      InfoBarService::FromWebContents(tab_android->web_contents()), file_path,
      delegate, download_info, is_incognito);
}

static void LaunchPermissionUpdateInfoBar(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jobject>& tab,
    const JavaParamRef<jstring>& jpermission,
    jlong callback_id) {
  TabAndroid* tab_android = TabAndroid::GetNativeTab(env, tab);

  std::string permission =
      base::android::ConvertJavaStringToUTF8(env, jpermission);

  // Convert java long long int to c++ pointer, take ownership.
  static_assert(
      std::is_same<
          DownloadControllerBase::AcquireFileAccessPermissionCallback,
          base::Callback<void(bool)>>::value,
      "Callback types don't match!");
  std::unique_ptr<base::Callback<void(bool)>> cb(
      reinterpret_cast<base::Callback<void(bool)>*>(callback_id));

  std::vector<std::string> permissions;
  permissions.push_back(permission);

  PermissionUpdateInfoBarDelegate::Create(
      tab_android->web_contents(),
      permissions,
      IDS_MISSING_STORAGE_PERMISSION_DOWNLOAD_EDUCATION_TEXT,
      *cb);
}

ChromeDownloadDelegate::ChromeDownloadDelegate(
    WebContents* web_contents) {}

ChromeDownloadDelegate::~ChromeDownloadDelegate() {
   JNIEnv* env = base::android::AttachCurrentThread();
   env->DeleteGlobalRef(java_ref_);
}

void ChromeDownloadDelegate::SetJavaRef(JNIEnv* env, jobject jobj) {
  java_ref_ = env->NewGlobalRef(jobj);
}

void ChromeDownloadDelegate::OnDownloadStarted(const std::string& filename) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jfilename = ConvertUTF8ToJavaString(
      env, filename);
  Java_ChromeDownloadDelegate_onDownloadStarted(env, java_ref_, jfilename);
}

void ChromeDownloadDelegate::RequestFileAccess(intptr_t callback_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ChromeDownloadDelegate_requestFileAccess(
      env, java_ref_, callback_id);
}

void ChromeDownloadDelegate::EnqueueDownloadManagerRequest(
    const std::string& url,
    const std::string& user_agent,
    const base::string16& file_name,
    const std::string& mime_type,
    const std::string& cookie,
    const std::string& referer) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jurl =
      ConvertUTF8ToJavaString(env, url);
  ScopedJavaLocalRef<jstring> juser_agent =
      ConvertUTF8ToJavaString(env, user_agent);
  ScopedJavaLocalRef<jstring> jmime_type =
      ConvertUTF8ToJavaString(env, mime_type);
  ScopedJavaLocalRef<jstring> jcookie =
      ConvertUTF8ToJavaString(env, cookie);
  ScopedJavaLocalRef<jstring> jreferer =
      ConvertUTF8ToJavaString(env, referer);
  ScopedJavaLocalRef<jstring> jfile_name =
      base::android::ConvertUTF16ToJavaString(env, file_name);
  Java_ChromeDownloadDelegate_enqueueAndroidDownloadManagerRequest(
      env, java_ref_, jurl, juser_agent, jfile_name, jmime_type, jcookie,
      jreferer);
}

void Init(JNIEnv* env,
          const JavaParamRef<jobject>& obj,
          const JavaParamRef<jobject>& jweb_contents) {
  auto* web_contents = WebContents::FromJavaWebContents(jweb_contents);
  ChromeDownloadDelegate::CreateForWebContents(web_contents);
  ChromeDownloadDelegate::FromWebContents(web_contents)->SetJavaRef(env, obj);
}

bool RegisterChromeDownloadDelegate(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

DEFINE_WEB_CONTENTS_USER_DATA_KEY(ChromeDownloadDelegate);

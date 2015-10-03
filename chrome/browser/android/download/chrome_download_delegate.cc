// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/download/chrome_download_delegate.h"

#include <jni.h>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/android/download/android_download_manager_overwrite_infobar_delegate.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/download/download_extensions.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/permissions/permission_update_infobar_delegate_android.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/android/download_controller_android.h"
#include "jni/ChromeDownloadDelegate_jni.h"
#include "ui/base/l10n/l10n_util.h"

using content::DownloadControllerAndroid;

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

// Returns true if a file name is dangerous, or false otherwise.
static jboolean IsDownloadDangerous(JNIEnv* env,
                                    const JavaParamRef<jclass>& clazz,
                                    const JavaParamRef<jstring>& filename) {
  base::FilePath path(base::android::ConvertJavaStringToUTF8(env, filename));
  return download_util::GetFileDangerLevel(path) !=
      download_util::NOT_DANGEROUS;
}

// Called when a dangerous download is validated.
static void DangerousDownloadValidated(JNIEnv* env,
                                       const JavaParamRef<jclass>& clazz,
                                       const JavaParamRef<jobject>& tab,
                                       jint download_id,
                                       jboolean accept) {
  TabAndroid* tab_android = TabAndroid::GetNativeTab(env, tab);
  DownloadControllerAndroid::Get()->DangerousDownloadValidated(
      tab_android->web_contents(), download_id, accept);
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

// Called when we need to interrupt download and ask users whether to overwrite
// an existing file.
static void LaunchDownloadOverwriteInfoBar(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jobject>& delegate,
    const JavaParamRef<jobject>& tab,
    const JavaParamRef<jobject>& download_info,
    const JavaParamRef<jstring>& jfile_name,
    const JavaParamRef<jstring>& jdir_name,
    const JavaParamRef<jstring>& jdir_full_path) {
  TabAndroid* tab_android = TabAndroid::GetNativeTab(env, tab);

  std::string file_name =
      base::android::ConvertJavaStringToUTF8(env, jfile_name);
  std::string dir_name = base::android::ConvertJavaStringToUTF8(env, jdir_name);
  std::string dir_full_path =
      base::android::ConvertJavaStringToUTF8(env, jdir_full_path);

  chrome::android::AndroidDownloadManagerOverwriteInfoBarDelegate::Create(
      InfoBarService::FromWebContents(tab_android->web_contents()), file_name,
      dir_name, dir_full_path, delegate, download_info);
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
  scoped_ptr<DownloadControllerAndroid::AcquireFileAccessPermissionCallback> cb(
      reinterpret_cast<
          DownloadControllerAndroid::AcquireFileAccessPermissionCallback*>(
              callback_id));

  std::vector<std::string> permissions;
  permissions.push_back(permission);

  PermissionUpdateInfoBarDelegate::Create(
      tab_android->web_contents(),
      permissions,
      IDS_MISSING_STORAGE_PERMISSION_DOWNLOAD_EDUCATION_TEXT,
      *cb);
}

bool RegisterChromeDownloadDelegate(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

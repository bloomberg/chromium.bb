// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/download/chrome_download_delegate.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/files/file_path.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/download/download_extensions.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/android/download_controller_android.h"
#include "jni/ChromeDownloadDelegate_jni.h"
#include "ui/base/l10n/l10n_util.h"

bool RegisterChromeDownloadDeleagte(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// Gets the download warning text for the given file name.
static jstring GetDownloadWarningText(
    JNIEnv* env, jclass clazz, jstring filename) {
  return base::android::ConvertUTF8ToJavaString(
      env, l10n_util::GetStringFUTF8(
          IDS_PROMPT_DANGEROUS_DOWNLOAD,
          base::android::ConvertJavaStringToUTF16(env, filename))).Release();
}

// Returns true if a file name is dangerous, or false otherwise.
static jboolean IsDownloadDangerous(
    JNIEnv* env, jclass clazz, jstring filename) {
  base::FilePath path(base::android::ConvertJavaStringToUTF8(env, filename));
  return download_util::GetFileDangerLevel(path) !=
      download_util::NOT_DANGEROUS;
}

// Called when a dangerous download is validated.
static void DangerousDownloadValidated(
    JNIEnv* env, jclass clazz, jobject tab, jint download_id, jboolean accept) {
  TabAndroid* tab_android = TabAndroid::GetNativeTab(env, tab);
  content::DownloadControllerAndroid::Get()->DangerousDownloadValidated(
      tab_android->web_contents(), download_id, accept);
}

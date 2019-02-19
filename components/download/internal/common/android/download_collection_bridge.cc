// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/common/android/download_collection_bridge.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/files/file_util.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "jni/DownloadCollectionBridge_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace download {

// static
base::FilePath DownloadCollectionBridge::CreateIntermediateUriForPublish(
    const GURL& original_url,
    const GURL& referrer_url,
    const base::FilePath& file_name,
    const std::string& mime_type) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jurl =
      ConvertUTF8ToJavaString(env, original_url.spec());
  ScopedJavaLocalRef<jstring> jreferrer =
      ConvertUTF8ToJavaString(env, referrer_url.spec());
  ScopedJavaLocalRef<jstring> jfile_name =
      ConvertUTF16ToJavaString(env, file_name.AsUTF16Unsafe());
  ScopedJavaLocalRef<jstring> jmime_type =
      ConvertUTF8ToJavaString(env, mime_type);
  ScopedJavaLocalRef<jstring> jcontent_uri =
      Java_DownloadCollectionBridge_createIntermediateUriForPublish(
          env, jfile_name, jmime_type, jurl, jreferrer);
  if (jcontent_uri) {
    std::string content_uri = ConvertJavaStringToUTF8(env, jcontent_uri);
    return base::FilePath(content_uri);
  }
  return base::FilePath();
}

// static
bool DownloadCollectionBridge::ShouldPublishDownload(
    const base::FilePath& file_path) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jfile_path =
      ConvertUTF16ToJavaString(env, file_path.AsUTF16Unsafe());
  return Java_DownloadCollectionBridge_shouldPublishDownload(env, jfile_path);
}

// static
DownloadInterruptReason DownloadCollectionBridge::MoveFileToIntermediateUri(
    const base::FilePath& source_path,
    const base::FilePath& destination_uri) {
  DCHECK(!source_path.IsContentUri());
  DCHECK(destination_uri.IsContentUri());

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jsource =
      ConvertUTF8ToJavaString(env, source_path.value());
  ScopedJavaLocalRef<jstring> jdestination =
      ConvertUTF8ToJavaString(env, destination_uri.value());
  bool success = Java_DownloadCollectionBridge_copyFileToIntermediateUri(
      env, jsource, jdestination);
  base::DeleteFile(source_path, false);
  return success ? DOWNLOAD_INTERRUPT_REASON_NONE
                 : DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;
}

}  // namespace download

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/download/download_media_parser_bridge.h"

#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/files/file_path.h"
#include "base/task/post_task.h"
#include "jni/DownloadMediaParserBridge_jni.h"

namespace {

void OnMediaParsed(const base::android::ScopedJavaGlobalRef<jobject> jcallback,
                   bool success) {
  base::android::RunBooleanCallbackAndroid(jcallback, success);
}

}  // namespace

// static
jlong JNI_DownloadMediaParserBridge_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jstring>& jmime_type,
    const base::android::JavaParamRef<jstring>& jfile_path,
    jlong jtotal_size,
    const base::android::JavaParamRef<jobject>& jcallback) {
  base::FilePath file_path(
      base::android::ConvertJavaStringToUTF8(env, jfile_path));
  std::string mime_type =
      base::android::ConvertJavaStringToUTF8(env, jmime_type);

  auto* bridge = new DownloadMediaParserBridge(
      static_cast<int64_t>(jtotal_size), mime_type, file_path,
      base::BindOnce(&OnMediaParsed,
                     base::android::ScopedJavaGlobalRef<jobject>(jcallback)));
  return reinterpret_cast<intptr_t>(bridge);
}

DownloadMediaParserBridge::DownloadMediaParserBridge(
    int64_t size,
    const std::string& mime_type,
    const base::FilePath& file_path,
    DownloadMediaParser::ParseCompleteCB parse_complete_cb)
    : parser_(std::make_unique<DownloadMediaParser>(
          size,
          mime_type,
          file_path,
          std::move(parse_complete_cb))) {}

DownloadMediaParserBridge::~DownloadMediaParserBridge() = default;

void DownloadMediaParserBridge::Destory(JNIEnv* env, jobject obj) {
  delete this;
}

void DownloadMediaParserBridge::Start(JNIEnv* env, jobject obj) {
  parser_->Start();
}

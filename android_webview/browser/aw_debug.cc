// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/common/aw_channel.h"
#include "android_webview/common/crash_reporter/aw_crash_reporter_client.h"
#include "android_webview/common/crash_reporter/crash_keys.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/path_utils.h"
#include "base/debug/dump_without_crashing.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/threading/thread_restrictions.h"
#include "components/crash/content/app/crash_reporter_client.h"
#include "components/crash/core/common/crash_key.h"
#include "components/version_info/version_info.h"
#include "components/version_info/version_info_values.h"
#include "jni/AwDebug_jni.h"

using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace android_webview {

namespace {

class AwDebugCrashReporterClient
    : public ::crash_reporter::CrashReporterClient {
 public:
  AwDebugCrashReporterClient() = default;
  ~AwDebugCrashReporterClient() override = default;

  void GetProductNameAndVersion(std::string* product_name,
                                std::string* version,
                                std::string* channel) override {
    *product_name = "AndroidWebView";
    *version = PRODUCT_VERSION;
    *channel =
        version_info::GetChannelString(android_webview::GetChannelOrStable());
  }

  bool GetCrashDumpLocation(base::FilePath* debug_dir) override {
    base::FilePath cache_dir;
    if (!base::android::GetCacheDirectory(&cache_dir)) {
      return false;
    }
    *debug_dir = cache_dir.Append(FILE_PATH_LITERAL("WebView")).Append("Debug");
    return true;
  }

  void GetSanitizationInformation(const char* const** annotations_whitelist,
                                  void** target_module,
                                  bool* sanitize_stacks) override {
    *annotations_whitelist = crash_keys::kWebViewCrashKeyWhiteList;
    *target_module = nullptr;
    *sanitize_stacks = true;
  }

  DISALLOW_COPY_AND_ASSIGN(AwDebugCrashReporterClient);
};

}  // namespace

static jboolean JNI_AwDebug_DumpWithoutCrashing(
    JNIEnv* env,
    const JavaParamRef<jstring>& dump_path) {
  // This may be called from any thread, and we might be in a state
  // where it is impossible to post tasks, so we have to be prepared
  // to do IO from this thread.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  base::File target(base::FilePath(ConvertJavaStringToUTF8(env, dump_path)),
                    base::File::FLAG_OPEN_TRUNCATED | base::File::FLAG_READ |
                        base::File::FLAG_WRITE);
  if (!target.IsValid())
    return false;
  // breakpad_linux::HandleCrashDump will close this fd once it is done.
  return crash_reporter::DumpWithoutCrashingToFd(target.TakePlatformFile());
}

static void JNI_AwDebug_InitCrashKeysForWebViewTesting(JNIEnv* env) {
  crash_keys::InitCrashKeysForWebViewTesting();
}

static void JNI_AwDebug_SetWhiteListedKeyForTesting(JNIEnv* env) {
  static ::crash_reporter::CrashKeyString<32> crash_key(
      "AW_WHITELISTED_DEBUG_KEY");
  crash_key.Set("AW_DEBUG_VALUE");
}

static void JNI_AwDebug_SetNonWhiteListedKeyForTesting(JNIEnv* env) {
  static ::crash_reporter::CrashKeyString<32> crash_key(
      "AW_NONWHITELISTED_DEBUG_KEY");
  crash_key.Set("AW_DEBUG_VALUE");
}

}  // namespace android_webview

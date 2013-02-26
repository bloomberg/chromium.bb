// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/android_browser_process.h"

#include "base/android/jni_string.h"
#include "base/debug/debugger.h"
#include "base/logging.h"
#include "content/browser/android/content_startup_flags.h"
#include "content/public/common/content_constants.h"
#include "jni/AndroidBrowserProcess_jni.h"

using base::android::ConvertJavaStringToUTF8;

namespace content {

static void SetCommandLineFlags(JNIEnv*env,
                                jclass clazz,
                                jint max_render_process_count,
                                jstring plugin_descriptor) {
  std::string plugin_str = (plugin_descriptor == NULL ?
      std::string() : ConvertJavaStringToUTF8(env, plugin_descriptor));
  SetContentCommandLineFlags(max_render_process_count, plugin_str);
}

static jboolean IsOfficialBuild(JNIEnv* env, jclass clazz) {
#if defined(OFFICIAL_BUILD)
  return true;
#else
  return false;
#endif
}

static jboolean IsPluginEnabled(JNIEnv* env, jclass clazz) {
#if defined(ENABLE_PLUGINS)
  return true;
#else
  return false;
#endif
}

bool RegisterAndroidBrowserProcess(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace

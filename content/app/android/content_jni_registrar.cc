// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/android/content_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "content/app/android/content_main.h"
#include "content/browser/android/android_browser_process.h"
#include "content/browser/android/command_line.h"
#include "content/browser/android/device_info.h"
#include "content/browser/android/download_controller.h"
#include "content/browser/android/jni_helper.h"
#include "content/browser/android/trace_event_binding.h"

namespace content {
namespace android {

base::android::RegistrationMethod kContentRegisteredMethods[] = {
  { "AndroidBrowserProcess", content::RegisterAndroidBrowserProcess },
  { "CommandLine", RegisterCommandLine },
  { "ContentMain", content::RegisterContentMain },
  { "DeviceInfo", RegisterDeviceInfo },
  { "DownloadController", DownloadController::RegisterDownloadController },
  { "JniHelper", RegisterJniHelper },
  { "TraceEvent", RegisterTraceEvent },
};

bool RegisterJni(JNIEnv* env) {
  return RegisterNativeMethods(env, kContentRegisteredMethods,
                               arraysize(kContentRegisteredMethods));
}

}  // namespace android
}  // namespace content

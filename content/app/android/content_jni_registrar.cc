// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/android/content_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "content/app/android/content_main.h"
#include "content/app/android/sandboxed_process_service.h"
#include "content/browser/android/android_browser_process.h"
#include "content/browser/android/content_view_client.h"
#include "content/browser/android/content_view_impl.h"
#include "content/browser/android/device_info.h"
#include "content/browser/android/download_controller.h"
#include "content/browser/android/sandboxed_process_launcher.h"
#include "content/browser/android/touch_point.h"
#include "content/common/android/command_line.h"
#include "content/common/android/surface_callback.h"
#include "content/common/android/trace_event_binding.h"

namespace content {
namespace android {

base::android::RegistrationMethod kContentRegisteredMethods[] = {
  { "AndroidBrowserProcess", content::RegisterAndroidBrowserProcess },
  { "CommandLine", RegisterCommandLine },
  { "ContentView", RegisterContentView },
  { "ContentViewClient", RegisterContentViewClient },
  { "ContentMain", content::RegisterContentMain },
  { "DeviceInfo", RegisterDeviceInfo },
  { "DownloadController", DownloadController::RegisterDownloadController },
  { "SandboxedProcessLauncher", content::RegisterSandboxedProcessLauncher },
  { "SandboxedProcessService", content::RegisterSandboxedProcessService },
  { "SurfaceCallback", content::RegisterSurfaceCallback },
  { "TouchPoint", content::RegisterTouchPoint },
  { "TraceEvent", RegisterTraceEvent },
};

bool RegisterJni(JNIEnv* env) {
  return RegisterNativeMethods(env, kContentRegisteredMethods,
                               arraysize(kContentRegisteredMethods));
}

}  // namespace android
}  // namespace content

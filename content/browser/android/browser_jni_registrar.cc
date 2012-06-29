// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/browser_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "content/browser/android/android_browser_process.h"
#include "content/browser/android/content_settings.h"
#include "content/browser/android/content_view_client.h"
#include "content/browser/android/content_view_impl.h"
#include "content/browser/android/device_info.h"
#include "content/browser/android/download_controller.h"
#include "content/browser/android/sandboxed_process_launcher.h"
#include "content/browser/android/touch_point.h"
#include "content/browser/geolocation/location_api_adapter_android.h"

namespace {
base::android::RegistrationMethod kContentRegisteredMethods[] = {
  { "AndroidLocationApiAdapter",
    AndroidLocationApiAdapter::RegisterGeolocationService },
  { "AndroidBrowserProcess", content::RegisterAndroidBrowserProcess },
  { "ContentSettings", content::ContentSettings::RegisterContentSettings },
  { "ContentView", content::RegisterContentView },
  { "ContentViewClient", content::RegisterContentViewClient },
  { "DeviceInfo", content::RegisterDeviceInfo },
  { "DownloadController",
    content::DownloadController::RegisterDownloadController },
  { "SandboxedProcessLauncher", content::RegisterSandboxedProcessLauncher },
  { "TouchPoint", content::RegisterTouchPoint },
};

}  // namespace

namespace content {
namespace android {

bool RegisterBrowserJni(JNIEnv* env) {
  return RegisterNativeMethods(env, kContentRegisteredMethods,
                               arraysize(kContentRegisteredMethods));
}

}  // namespace android
}  // namespace content

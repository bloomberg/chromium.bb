// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/android/cast_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "chromecast/android/cast_metrics_helper_android.h"
#include "chromecast/app/android/crash_handler.h"
#include "chromecast/base/android/dumpstate_writer.h"
#include "chromecast/base/android/system_time_change_notifier_android.h"
#include "chromecast/base/cast_sys_info_android.h"
#include "chromecast/base/chromecast_config_android.h"
#include "chromecast/browser/android/cast_window_android.h"
#include "chromecast/browser/android/cast_window_manager.h"
#include "components/external_video_surface/component_jni_registrar.h"

namespace chromecast {
namespace android {

namespace {

static base::android::RegistrationMethod kMethods[] = {
  { "CastMetricsHelperAndroid", CastMetricsHelperAndroid::RegisterJni },
  { "CastSysInfoAndroid", CastSysInfoAndroid::RegisterJni },
  { "CastWindowAndroid", shell::CastWindowAndroid::RegisterJni },
  { "CastWindowManager", shell::RegisterCastWindowManager },
  { "ChromecastConfigAndroid", ChromecastConfigAndroid::RegisterJni },
  { "CrashHandler", CrashHandler::RegisterCastCrashJni },
  { "DumpstateWriter", DumpstateWriter::RegisterJni },
  { "ExternalVideoSurfaceContainer",
      external_video_surface::RegisterExternalVideoSurfaceJni },
  { "SystemTimeChangeNotifierAndroid",
      SystemTimeChangeNotifierAndroid::RegisterJni },
};

}  // namespace

bool RegisterJni(JNIEnv* env) {
  return RegisterNativeMethods(env, kMethods, arraysize(kMethods));
}

}  // namespace android
}  // namespace chromecast

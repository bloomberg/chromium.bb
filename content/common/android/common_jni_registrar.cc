// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/android/common_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "content/common/android/command_line.h"
#include "content/common/android/device_telephony_info.h"
#include "content/common/android/hash_set.h"
#include "content/common/android/surface_callback.h"
#include "content/common/android/surface_texture_listener.h"
#include "content/common/android/trace_event_binding.h"

namespace {
base::android::RegistrationMethod kContentRegisteredMethods[] = {
  { "CommandLine", RegisterCommandLine },
  { "DeviceTelephonyInfo",
        content::DeviceTelephonyInfo::RegisterDeviceTelephonyInfo },
  { "HashSet", content::RegisterHashSet },
  { "SurfaceCallback", content::RegisterSurfaceCallback },
  { "SurfaceTextureListener",
        content::SurfaceTextureListener::RegisterSurfaceTextureListener },
  { "TraceEvent", RegisterTraceEvent },
};

}  // namespace

namespace content {
namespace android {

bool RegisterCommonJni(JNIEnv* env) {
  return RegisterNativeMethods(env, kContentRegisteredMethods,
                               arraysize(kContentRegisteredMethods));
}

}  // namespace android
}  // namespace content

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/content_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "content/browser/android/command_line.h"
#include "content/browser/android/trace_event_binding.h"

namespace content {
namespace android {

base::android::RegistrationMethod kContentRegisteredMethods[] = {
  { "CommandLine", RegisterCommandLine },
  { "TraceEvent", RegisterTraceEvent },
};

bool RegisterJni(JNIEnv* env) {
  return RegisterNativeMethods(env, kContentRegisteredMethods,
                               arraysize(kContentRegisteredMethods));
}

}  // namespace android
}  // namespace content

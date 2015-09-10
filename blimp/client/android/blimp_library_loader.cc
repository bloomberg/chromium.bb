// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/android/blimp_library_loader.h"

#include <vector>

#include "base/android/base_jni_onload.h"
#include "base/android/base_jni_registrar.h"
#include "base/android/jni_android.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "blimp/client/android/blimp_jni_registrar.h"
#include "jni/BlimpLibraryLoader_jni.h"
#include "ui/gl/gl_surface.h"

namespace {

base::LazyInstance<scoped_ptr<base::MessageLoopForUI>> g_main_message_loop =
    LAZY_INSTANCE_INITIALIZER;

bool OnLibrariesLoaded(JNIEnv* env, jclass clazz) {
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  logging::InitLogging(settings);

  // Disable process info prefixes on log lines.  These can be obtained via "adb
  // logcat -v threadtime".
  logging::SetLogItems(false,   // Process ID
                       false,   // Thread ID
                       false,   // Timestamp
                       false);  // Tick count
  VLOG(0) << "Chromium logging enabled: level = " << logging::GetMinLogLevel()
          << ", default verbosity = " << logging::GetVlogVerbosity();

  return true;
}

bool OnJniInitializationComplete() {
  base::android::SetLibraryLoadedHook(&OnLibrariesLoaded);
  return true;
}

bool RegisterJni(JNIEnv* env) {
  if (!base::android::RegisterJni(env))
    return false;

  if (!blimp::RegisterBlimpJni(env))
    return false;

  return true;
}

}  // namespace

namespace blimp {

static jboolean InitializeBlimp(JNIEnv* env,
                                const JavaParamRef<jclass>& clazz,
                                const JavaParamRef<jobject>& jcontext) {
  base::android::InitApplicationContext(env, jcontext);

  // TODO(dtrainor): Start the runner?
  return true;
}

static jboolean StartBlimp(JNIEnv* env, const JavaParamRef<jclass>& clazz) {
  // TODO(dtrainor): Initialize ICU?

  if (!gfx::GLSurface::InitializeOneOff())
    return false;

  g_main_message_loop.Get().reset(new base::MessageLoopForUI);
  base::MessageLoopForUI::current()->Start();

  return true;
}

bool RegisterBlimpLibraryLoaderJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace blimp

JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  std::vector<base::android::RegisterCallback> register_callbacks;
  register_callbacks.push_back(base::Bind(&RegisterJni));

  std::vector<base::android::InitCallback> init_callbacks;
  init_callbacks.push_back(base::Bind(&OnJniInitializationComplete));

  // Although we only need to register JNI for base/ and blimp/, this follows
  // the general Chrome for Android pattern, to be future-proof against future
  // changes to JNI.
  if (!base::android::OnJNIOnLoadRegisterJNI(vm, register_callbacks) ||
      !base::android::OnJNIOnLoadInit(init_callbacks)) {
    return -1;
  }

  return JNI_VERSION_1_4;
}

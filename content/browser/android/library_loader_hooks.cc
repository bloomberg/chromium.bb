// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/android_library_loader_hooks.h"

#include "base/android/base_jni_registrar.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "base/tracked_objects.h"
#include "content/public/common/content_switches.h"
#include "media/base/android/media_jni_registrar.h"
#include "net/android/net_jni_registrar.h"

jboolean LibraryLoaderEntryHook(JNIEnv* env, jclass clazz,
                                jobjectArray init_command_line) {

  // TODO(tedchoc): Initialize the native command line from the java array
  // passed in.

  CommandLine* command_line = CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(switches::kTraceStartup)) {
    base::debug::TraceLog::GetInstance()->SetEnabled(
        command_line->GetSwitchValueASCII(switches::kTraceStartup));
  }

  // Can only use event tracing after setting up the command line.
  TRACE_EVENT0("jni", "JNI_OnLoad continuation");

  logging::InitLogging(NULL,
                       logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
                       logging::DONT_LOCK_LOG_FILE,
                       logging::DELETE_OLD_LOG_FILE,
                       logging::ENABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);
  // To view log output with IDs and timestamps use "adb logcat -v threadtime".
  logging::SetLogItems(false,    // Process ID
                       false,    // Thread ID
                       false,    // Timestamp
                       false);   // Tick count
  VLOG(0) << "Chromium logging enabled: level = " << logging::GetMinLogLevel()
          << ", default verbosity = " << logging::GetVlogVerbosity();

  if (!base::android::RegisterJni(env))
    return JNI_FALSE;

  if (!net::android::RegisterJni(env))
    return JNI_FALSE;

  if (!media::RegisterJni(env))
    return JNI_FALSE;

  return JNI_TRUE;
}

bool RegisterLibraryLoaderEntryHook(JNIEnv* env) {
  // TODO(bulach): use the jni generator once we move jni_helper methods here.
  const JNINativeMethod kMethods[] = {
      { "nativeLibraryLoadedOnMainThread", "([Ljava/lang/String;)Z",
          reinterpret_cast<void*>(LibraryLoaderEntryHook) },
  };
  const int kMethodsSize = arraysize(kMethods);
  // TODO(tedchoc): Upstream LibraryLoader.java and replace path to make this
  // work.
  const char kLibraryLoaderPath[] = "";
  base::android::ScopedJavaLocalRef<jclass> clazz =
      base::android::GetClass(env, kLibraryLoaderPath);

  if (env->RegisterNatives(clazz.obj(),
                           kMethods,
                           kMethodsSize) < 0) {
    return false;
  }
  return true;
}

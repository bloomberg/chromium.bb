// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/app/android_library_loader_hooks.h"

#include "base/android/base_jni_registrar.h"
#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/android/jni_string.h"
#include "base/at_exit.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/tracked_objects.h"
#include "content/app/android/app_jni_registrar.h"
#include "content/browser/android/browser_jni_registrar.h"
#include "content/common/android/command_line.h"
#include "content/common/android/common_jni_registrar.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "jni/LibraryLoader_jni.h"
#include "media/base/android/media_jni_registrar.h"
#include "net/android/net_jni_registrar.h"
#include "ui/android/ui_jni_registrar.h"
#include "ui/shell_dialogs/android/shell_dialogs_jni_registrar.h"

namespace {
base::AtExitManager* g_at_exit_manager = NULL;
}

namespace content {

static jint LibraryLoadedOnMainThread(JNIEnv* env, jclass clazz,
                                          jobjectArray init_command_line) {
  InitNativeCommandLineFromJavaArray(env, init_command_line);

  CommandLine* command_line = CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(switches::kTraceStartup)) {
    base::debug::TraceLog::GetInstance()->SetEnabled(
        command_line->GetSwitchValueASCII(switches::kTraceStartup),
        base::debug::TraceLog::RECORD_UNTIL_FULL);
  }

  // Can only use event tracing after setting up the command line.
  TRACE_EVENT0("jni", "JNI_OnLoad continuation");

  // Note: because logging is setup here right after copying the command line
  // array from java to native up top of this method, any code that adds the
  // --enable-dcheck switch must do so on the Java side.
  logging::DcheckState dcheck_state =
      command_line->HasSwitch(switches::kEnableDCHECK) ?
      logging::ENABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS :
      logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS;
  logging::InitLogging(NULL,
                       logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
                       logging::DONT_LOCK_LOG_FILE,
                       logging::DELETE_OLD_LOG_FILE,
                       dcheck_state);
  // To view log output with IDs and timestamps use "adb logcat -v threadtime".
  logging::SetLogItems(false,    // Process ID
                       false,    // Thread ID
                       false,    // Timestamp
                       false);   // Tick count
  VLOG(0) << "Chromium logging enabled: level = " << logging::GetMinLogLevel()
          << ", default verbosity = " << logging::GetVlogVerbosity();

  if (!base::android::RegisterJni(env))
    return RESULT_CODE_FAILED_TO_REGISTER_JNI;

  if (!net::android::RegisterJni(env))
    return RESULT_CODE_FAILED_TO_REGISTER_JNI;

  if (!ui::android::RegisterJni(env))
    return RESULT_CODE_FAILED_TO_REGISTER_JNI;

  if (!ui::shell_dialogs::RegisterJni(env))
    return RESULT_CODE_FAILED_TO_REGISTER_JNI;

  if (!content::android::RegisterCommonJni(env))
    return RESULT_CODE_FAILED_TO_REGISTER_JNI;

  if (!content::android::RegisterBrowserJni(env))
    return RESULT_CODE_FAILED_TO_REGISTER_JNI;

  if (!content::android::RegisterAppJni(env))
    return RESULT_CODE_FAILED_TO_REGISTER_JNI;

  if (!media::RegisterJni(env))
    return RESULT_CODE_FAILED_TO_REGISTER_JNI;

  return 0;
}

void LibraryLoaderExitHook() {
  if (g_at_exit_manager) {
    delete g_at_exit_manager;
    g_at_exit_manager = NULL;
  }
}

bool RegisterLibraryLoaderEntryHook(JNIEnv* env) {
  // We need the AtExitManager to be created at the very beginning.
  g_at_exit_manager = new base::AtExitManager();

  return RegisterNativesImpl(env);
}

}  // namespace content

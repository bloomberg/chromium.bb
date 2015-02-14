// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/android/library_loader_hooks.h"

#include "base/android/base_jni_registrar.h"
#include "base/android/command_line_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/android/jni_string.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "base/tracked_objects.h"
#include "content/app/android/app_jni_registrar.h"
#include "content/browser/android/browser_jni_registrar.h"
#include "content/common/android/common_jni_registrar.h"
#include "content/common/content_constants_internal.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "device/vibration/android/vibration_jni_registrar.h"
#include "media/base/android/media_jni_registrar.h"
#include "net/android/net_jni_registrar.h"
#include "ui/android/ui_android_jni_registrar.h"
#include "ui/base/android/ui_base_jni_registrar.h"
#include "ui/gfx/android/gfx_jni_registrar.h"
#include "ui/gl/android/gl_jni_registrar.h"
#include "ui/shell_dialogs/android/shell_dialogs_jni_registrar.h"

namespace content {

bool EnsureJniRegistered(JNIEnv* env) {
  static bool g_jni_init_done = false;

  if (!g_jni_init_done) {
    if (!base::android::RegisterJni(env))
      return false;

    if (!gfx::android::RegisterJni(env))
      return false;

    if (!net::android::RegisterJni(env))
      return false;

    if (!ui::android::RegisterJni(env))
      return false;

    if (!ui::gl::android::RegisterJni(env))
      return false;

    if (!ui::shell_dialogs::RegisterJni(env))
      return false;

    if (!content::android::RegisterCommonJni(env))
      return false;

    if (!content::android::RegisterBrowserJni(env))
      return false;

    if (!content::android::RegisterAppJni(env))
      return false;

    if (!device::android::RegisterVibrationJni(env))
      return false;

    if (!media::RegisterJni(env))
      return false;

    if (!ui::RegisterUIAndroidJni(env))
      return false;

    g_jni_init_done = true;
  }

  return true;
}

bool LibraryLoaded(JNIEnv* env, jclass clazz) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(switches::kTraceStartup)) {
    base::trace_event::CategoryFilter category_filter(
        command_line->GetSwitchValueASCII(switches::kTraceStartup));
    base::trace_event::TraceLog::GetInstance()->SetEnabled(
        category_filter,
        base::trace_event::TraceLog::RECORDING_MODE,
        base::trace_event::TraceOptions());
  }

  // Android's main browser loop is custom so we set the browser
  // name here as early as possible.
  TRACE_EVENT_BEGIN_ETW("BrowserMain", 0, "");
  base::trace_event::TraceLog::GetInstance()->SetProcessName("Browser");
  base::trace_event::TraceLog::GetInstance()->SetProcessSortIndex(
      kTraceEventBrowserProcessSortIndex);

  // Can only use event tracing after setting up the command line.
  TRACE_EVENT0("jni", "JNI_OnLoad continuation");

  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  logging::InitLogging(settings);
  // To view log output with IDs and timestamps use "adb logcat -v threadtime".
  logging::SetLogItems(false,    // Process ID
                       false,    // Thread ID
                       false,    // Timestamp
                       false);   // Tick count
  VLOG(0) << "Chromium logging enabled: level = " << logging::GetMinLogLevel()
          << ", default verbosity = " << logging::GetVlogVerbosity();

  return true;
}

}  // namespace content

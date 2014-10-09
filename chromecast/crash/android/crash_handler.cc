// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/crash/android/crash_handler.h"

#include <jni.h>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "breakpad/src/client/linux/handler/exception_handler.h"
#include "breakpad/src/client/linux/handler/minidump_descriptor.h"
#include "chromecast/common/version.h"
#include "chromecast/crash/android/cast_crash_reporter_client_android.h"
#include "components/crash/app/breakpad_linux.h"
#include "components/crash/app/crash_reporter_client.h"
#include "content/public/common/content_switches.h"
#include "jni/CastCrashHandler_jni.h"

namespace {

chromecast::CrashHandler* g_crash_handler = NULL;

// ExceptionHandler requires a HandlerCallback as a function pointer. This
// function exists to proxy into the global CrashHandler instance.
bool HandleCrash(const void* /* crash_context */,
                 size_t /* crash_context_size */,
                 void* /* context */) {
  DCHECK(g_crash_handler);
  if (g_crash_handler->CanUploadCrashDump()) {
    g_crash_handler->AttemptUploadCrashDump();
  } else {
    g_crash_handler->RemoveCrashDumps();
  }

  // Let the exception continue to propagate up to the system.
  return false;
}

}  // namespace

namespace chromecast {

// static
void CrashHandler::Initialize(const std::string& process_type,
                              const base::FilePath& log_file_path) {
  DCHECK(!g_crash_handler);
  g_crash_handler = new CrashHandler(log_file_path);
  g_crash_handler->Initialize(process_type);
}

// static
bool CrashHandler::GetCrashDumpLocation(base::FilePath* crash_dir) {
  DCHECK(g_crash_handler);
  return g_crash_handler->crash_reporter_client_->
      GetCrashDumpLocation(crash_dir);
}

// static
bool CrashHandler::RegisterCastCrashJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

CrashHandler::CrashHandler(const base::FilePath& log_file_path)
    : log_file_path_(log_file_path),
      crash_reporter_client_(new CastCrashReporterClientAndroid) {
  if (!crash_reporter_client_->GetCrashDumpLocation(&crash_dump_path_)) {
    LOG(ERROR) << "Could not get crash dump location";
  }
  SetCrashReporterClient(crash_reporter_client_.get());
}

CrashHandler::~CrashHandler() {
  DCHECK(g_crash_handler);
  g_crash_handler = NULL;
}

void CrashHandler::Initialize(const std::string& process_type) {
  if (process_type != switches::kZygoteProcess) {
    if (process_type.empty()) {
      // ExceptionHandlers are called on crash in reverse order of
      // instantiation. This ExceptionHandler will attempt to upload crashes
      // and the log file written out by the main process.

      // Dummy MinidumpDescriptor just to start up another ExceptionHandler.
      google_breakpad::MinidumpDescriptor dummy(crash_dump_path_.value());
      crash_uploader_.reset(new google_breakpad::ExceptionHandler(
          dummy, NULL, NULL, NULL, true, -1));
      crash_uploader_->set_crash_handler(&::HandleCrash);

      breakpad::InitCrashReporter(process_type);
    } else {
      breakpad::InitNonBrowserCrashReporterForAndroid(process_type);
    }
  }

  UploadCrashDumpsAsync();
}

bool CrashHandler::CanUploadCrashDump() {
  DCHECK(crash_reporter_client_);
  return crash_reporter_client_->GetCollectStatsConsent();
}

void CrashHandler::AttemptUploadCrashDump() {
  VLOG(1) << "Attempting to upload current process crash";
  JNIEnv* env = base::android::AttachCurrentThread();
  // Crash dump location
  base::android::ScopedJavaLocalRef<jstring> crash_dump_path_java =
      base::android::ConvertUTF8ToJavaString(env,
                                             crash_dump_path_.value());
  // Current log file location
  base::android::ScopedJavaLocalRef<jstring> log_file_path_java =
      base::android::ConvertUTF8ToJavaString(env, log_file_path_.value());
  Java_CastCrashHandler_uploadCurrentProcessDumpSync(
      env,
      crash_dump_path_java.obj(),
      log_file_path_java.obj(),
      CAST_IS_DEBUG_BUILD);
}

void CrashHandler::UploadCrashDumpsAsync() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> crash_dump_path_java =
      base::android::ConvertUTF8ToJavaString(env,
                                             crash_dump_path_.value());
  Java_CastCrashHandler_uploadCrashDumpsAsync(env,
                                              crash_dump_path_java.obj(),
                                              CAST_IS_DEBUG_BUILD);
}

void CrashHandler::RemoveCrashDumps() {
  VLOG(1) << "Removing crash dumps instead of uploading";
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> crash_dump_path_java =
      base::android::ConvertUTF8ToJavaString(env,
                                             crash_dump_path_.value());
  Java_CastCrashHandler_removeCrashDumpsSync(
      env, crash_dump_path_java.obj(), CAST_IS_DEBUG_BUILD);
}

}  // namespace chromecast

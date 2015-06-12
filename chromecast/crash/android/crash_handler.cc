// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/crash/android/crash_handler.h"

#include <jni.h>
#include <stdlib.h>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "breakpad/src/client/linux/handler/exception_handler.h"
#include "breakpad/src/client/linux/handler/minidump_descriptor.h"
#include "chromecast/base/version.h"
#include "chromecast/crash/android/cast_crash_reporter_client_android.h"
#include "components/crash/app/breakpad_linux.h"
#include "components/crash/app/crash_reporter_client.h"
#include "content/public/common/content_switches.h"
#include "jni/CastCrashHandler_jni.h"

namespace {

chromecast::CrashHandler* g_crash_handler = NULL;

bool HandleCrash(void* /* crash_context */) {
  DCHECK(g_crash_handler);
  g_crash_handler->UploadCrashDumps();

  // TODO(gunsch): clean up the ATV crash handling code.
  // Don't write another minidump. Chrome's default ExceptionHandler has already
  // written a minidump by this point in the crash handling sequence.
  return false;
}

// Debug builds: always to crash-staging
// Release builds: only to crash-staging for local/invalid build numbers
bool UploadCrashToStaging() {
#if CAST_IS_DEBUG_BUILD()
  return true;
#else
  int build_number;
  if (base::StringToInt(CAST_BUILD_INCREMENTAL, &build_number))
    return build_number == 0;
  return true;
#endif
}

}  // namespace

namespace chromecast {

// static
void CrashHandler::Initialize(const std::string& process_type,
                              const base::FilePath& log_file_path) {
  DCHECK(!g_crash_handler);
  g_crash_handler = new CrashHandler(process_type, log_file_path);
  g_crash_handler->Initialize();
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

CrashHandler::CrashHandler(const std::string& process_type,
                           const base::FilePath& log_file_path)
    : log_file_path_(log_file_path),
      process_type_(process_type),
      crash_reporter_client_(new CastCrashReporterClientAndroid(process_type)) {
  if (!crash_reporter_client_->GetCrashDumpLocation(&crash_dump_path_)) {
    LOG(ERROR) << "Could not get crash dump location";
  }
  SetCrashReporterClient(crash_reporter_client_.get());
}

CrashHandler::~CrashHandler() {
  DCHECK(g_crash_handler);
  g_crash_handler = NULL;
}

void CrashHandler::Initialize() {
  if (process_type_.empty()) {
    InitializeUploader();

    // ExceptionHandlers are called on crash in reverse order of
    // instantiation. This ExceptionHandler will attempt to upload crashes
    // and the log file written out by the main process.

    // Dummy MinidumpDescriptor just to start up another ExceptionHandler.
    google_breakpad::MinidumpDescriptor dummy(crash_dump_path_.value());
    crash_uploader_.reset(new google_breakpad::ExceptionHandler(
        dummy, &HandleCrash, NULL, NULL, true, -1));

    breakpad::InitCrashReporter(process_type_);

    return;
  }

  if (process_type_ != switches::kZygoteProcess) {
    breakpad::InitNonBrowserCrashReporterForAndroid(process_type_);
  }
}

void CrashHandler::InitializeUploader() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> crash_dump_path_java =
      base::android::ConvertUTF8ToJavaString(env,
                                             crash_dump_path_.value());
  Java_CastCrashHandler_initializeUploader(
      env, crash_dump_path_java.obj(), UploadCrashToStaging());
}

bool CrashHandler::CanUploadCrashDump() {
  DCHECK(crash_reporter_client_);
  return crash_reporter_client_->GetCollectStatsConsent();
}

void CrashHandler::UploadCrashDumps() {
  VLOG(1) << "Attempting to upload current process crash";

  if (CanUploadCrashDump()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    // Current log file location
    base::android::ScopedJavaLocalRef<jstring> log_file_path_java =
        base::android::ConvertUTF8ToJavaString(env, log_file_path_.value());
    Java_CastCrashHandler_uploadCrashDumps(env, log_file_path_java.obj());
  } else {
    VLOG(1) << "Removing crash dumps instead of uploading";
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_CastCrashHandler_removeCrashDumps(env);
  }
}

}  // namespace chromecast

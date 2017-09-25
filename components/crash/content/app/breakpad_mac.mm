// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/crash/content/app/breakpad_mac.h"

#include <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#include <stddef.h>
#include <string.h>

#include "base/auto_reset.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#import "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#import "base/mac/scoped_nsautorelease_pool.h"
#include "base/macros.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#import "breakpad/src/client/mac/Framework/Breakpad.h"
#include "breakpad/src/common/simple_string_dictionary.h"
#include "components/crash/content/app/crash_reporter_client.h"
#include "components/crash/core/common/crash_keys.h"

using crash_reporter::GetCrashReporterClient;

namespace breakpad {

namespace {

BreakpadRef gBreakpadRef = NULL;

void SetCrashKeyValue(NSString* key, NSString* value) {
  // Comment repeated from header to prevent confusion:
  // IMPORTANT: On OS X, the key/value pairs are sent to the crash server
  // out of bounds and not recorded on disk in the minidump, this means
  // that if you look at the minidump file locally you won't see them!
  if (gBreakpadRef == NULL) {
    return;
  }

  BreakpadAddUploadParameter(gBreakpadRef, key, value);
}

void ClearCrashKeyValue(NSString* key) {
  if (gBreakpadRef == NULL) {
    return;
  }

  BreakpadRemoveUploadParameter(gBreakpadRef, key);
}

void SetCrashKeyValueImpl(const base::StringPiece& key,
                          const base::StringPiece& value) {
  @autoreleasepool {
    SetCrashKeyValue(base::SysUTF8ToNSString(key.as_string()),
                     base::SysUTF8ToNSString(value.as_string()));
  }
}

void ClearCrashKeyValueImpl(const base::StringPiece& key) {
  @autoreleasepool {
    ClearCrashKeyValue(base::SysUTF8ToNSString(key.as_string()));
  }
}

bool FatalMessageHandler(int severity, const char* file, int line,
                         size_t message_start, const std::string& str) {
  // Do not handle non-FATAL.
  if (severity != logging::LOG_FATAL)
    return false;

  // In case of OOM condition, this code could be reentered when
  // constructing and storing the key.  Using a static is not
  // thread-safe, but if multiple threads are in the process of a
  // fatal crash at the same time, this should work.
  static bool guarded = false;
  if (guarded)
    return false;

  base::AutoReset<bool> guard(&guarded, true);

  // Only log last path component.  This matches logging.cc.
  if (file) {
    const char* slash = strrchr(file, '/');
    if (slash)
      file = slash + 1;
  }

  NSString* fatal_key = @"LOG_FATAL";
  NSString* fatal_value =
      [NSString stringWithFormat:@"%s:%d: %s",
                                 file, line, str.c_str() + message_start];
  SetCrashKeyValue(fatal_key, fatal_value);

  // Rather than including the code to force the crash here, allow the
  // caller to do it.
  return false;
}

// BreakpadGenerateAndSendReport() does not report the current
// thread.  This class can be used to spin up a thread to run it.
class DumpHelper : public base::PlatformThread::Delegate {
 public:
  static void DumpWithoutCrashing() {
    DumpHelper dumper;
    base::PlatformThreadHandle handle;
    if (base::PlatformThread::Create(0, &dumper, &handle)) {
      // The entire point of this is to block so that the correct
      // stack is logged.
      base::ThreadRestrictions::ScopedAllowIO allow_io;
      base::PlatformThread::Join(handle);
    }
  }

 private:
  DumpHelper() {}

  void ThreadMain() override {
    base::PlatformThread::SetName("CrDumpHelper");
    BreakpadGenerateAndSendReport(gBreakpadRef);
  }

  DISALLOW_COPY_AND_ASSIGN(DumpHelper);
};

void SIGABRTHandler(int signal) {
  // The OSX abort() (link below) masks all signals for the process,
  // and all except SIGABRT for the thread.  SIGABRT will be masked
  // when the SIGABRT is sent, which means at this point only SIGKILL
  // and SIGSTOP can be delivered.  Unmask others so that the code
  // below crashes as desired.
  //
  // http://www.opensource.apple.com/source/Libc/Libc-825.26/stdlib/FreeBSD/abort.c
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, signal);
  pthread_sigmask(SIG_SETMASK, &mask, NULL);

  // Most interesting operations are not safe in a signal handler, just crash.
  char* volatile death_ptr = NULL;
  *death_ptr = '!';
}

}  // namespace

bool IsCrashReporterEnabled() {
  return gBreakpadRef != NULL;
}

// Only called for a branded build of Chrome.app.
void InitCrashReporter(const std::string& process_type) {
  // The maximum lengths specified by breakpad include the trailing NULL, so the
  // actual length of the chunk is one less.
  static_assert(google_breakpad::SimpleStringDictionary::value_size - 1 ==
                crash_keys::kChunkMaxLength, "kChunkMaxLength mismatch");
  static_assert(crash_keys::kSmallSize <= crash_keys::kChunkMaxLength,
                "crash key chunk size too small for small values");
  static_assert(crash_keys::kMediumSize <= crash_keys::kChunkMaxLength,
                "crash key chunk size too small for medium values");
  DCHECK(!gBreakpadRef);
  base::mac::ScopedNSAutoreleasePool autorelease_pool;

  // Check whether crash reporting should be enabled. If enterprise
  // configuration management controls crash reporting, it takes precedence.
  // Otherwise, check whether the user has consented to stats and crash
  // reporting. The browser process can make this determination directly.
  // Helper processes may not have access to the disk or to the same data as
  // the browser process, so the browser passes the decision to them on the
  // command line.
  NSBundle* main_bundle = base::mac::FrameworkBundle();
  bool is_browser = !base::mac::IsBackgroundOnlyProcess();
  bool enable_breakpad = false;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  if (is_browser) {
    // Since the configuration management infrastructure is possibly not
    // initialized when this code runs, read the policy preference directly.
    if (!GetCrashReporterClient()->ReportingIsEnforcedByPolicy(
            &enable_breakpad)) {
      // Controlled by the user. The crash reporter may be enabled by
      // preference or through an environment variable, but the kDisableBreakpad
      // switch overrides both.
      enable_breakpad = GetCrashReporterClient()->GetCollectStatsConsent() ||
                        GetCrashReporterClient()->IsRunningUnattended();
      enable_breakpad &= !command_line->HasSwitch(switches::kDisableBreakpad);
    }
  } else {
    // This is a helper process, check the command line switch.
    enable_breakpad = command_line->HasSwitch(switches::kEnableCrashReporter);
  }

  if (!enable_breakpad) {
    VLOG_IF(1, is_browser) << "Breakpad disabled";
    return;
  }

  // Tell Breakpad where crash_inspector and crash_report_sender are.
  NSString* resource_path = [main_bundle resourcePath];
  NSString *inspector_location =
      [resource_path stringByAppendingPathComponent:@"crash_inspector"];
  NSString *reporter_bundle_location =
      [resource_path stringByAppendingPathComponent:@"crash_report_sender.app"];
  NSString *reporter_location =
      [[NSBundle bundleWithPath:reporter_bundle_location] executablePath];

  if (!inspector_location || !reporter_location) {
    VLOG_IF(1, is_browser && base::mac::AmIBundled()) << "Breakpad disabled";
    return;
  }

  NSDictionary* info_dictionary = [main_bundle infoDictionary];
  NSMutableDictionary *breakpad_config =
      [[info_dictionary mutableCopy] autorelease];
  [breakpad_config setObject:inspector_location
                      forKey:@BREAKPAD_INSPECTOR_LOCATION];
  [breakpad_config setObject:reporter_location
                      forKey:@BREAKPAD_REPORTER_EXE_LOCATION];

  // In the main application (the browser process), crashes can be passed to
  // the system's Crash Reporter.  This allows the system to notify the user
  // when the application crashes, and provide the user with the option to
  // restart it.
  if (is_browser)
    [breakpad_config setObject:@"NO" forKey:@BREAKPAD_SEND_AND_EXIT];

  base::FilePath dir_crash_dumps;
  GetCrashReporterClient()->GetCrashDumpLocation(&dir_crash_dumps);
  [breakpad_config setObject:base::SysUTF8ToNSString(dir_crash_dumps.value())
                      forKey:@BREAKPAD_DUMP_DIRECTORY];

  // Temporarily run Breakpad in-process on 10.10 and later because APIs that
  // it depends on got broken (http://crbug.com/386208).
  // This can catch crashes in the browser process only.
  if (is_browser && base::mac::IsAtLeastOS10_10()) {
    [breakpad_config setObject:[NSNumber numberWithBool:YES]
                        forKey:@BREAKPAD_IN_PROCESS];
  }

  // Initialize Breakpad.
  gBreakpadRef = BreakpadCreate(breakpad_config);
  if (!gBreakpadRef) {
    LOG_IF(ERROR, base::mac::AmIBundled()) << "Breakpad initialization failed";
    return;
  }

  // Initialize the scoped crash key system.
  base::debug::SetCrashKeyReportingFunctions(&SetCrashKeyValueImpl,
                                             &ClearCrashKeyValueImpl);
  GetCrashReporterClient()->RegisterCrashKeys();

  // Set Breakpad metadata values.  These values are added to Info.plist during
  // the branded Google Chrome.app build.
  SetCrashKeyValue(@"ver", [info_dictionary objectForKey:@BREAKPAD_VERSION]);
  SetCrashKeyValue(@"prod", [info_dictionary objectForKey:@BREAKPAD_PRODUCT]);
  SetCrashKeyValue(@"plat", @"OS X");

  logging::SetLogMessageHandler(&FatalMessageHandler);
  base::debug::SetDumpWithoutCrashingFunction(&DumpHelper::DumpWithoutCrashing);

  // abort() sends SIGABRT, which breakpad does not intercept.
  // Register a signal handler to crash in a way breakpad will
  // intercept.
  struct sigaction sigact;
  memset(&sigact, 0, sizeof(sigact));
  sigact.sa_handler = SIGABRTHandler;
  CHECK(0 == sigaction(SIGABRT, &sigact, NULL));
}

void InitCrashProcessInfo(const std::string& process_type_switch) {
  if (gBreakpadRef == NULL) {
    return;
  }

  // Determine the process type.
  NSString* process_type = @"browser";
  if (!process_type_switch.empty()) {
    process_type = base::SysUTF8ToNSString(process_type_switch);
  }

  // Store process type in crash dump.
  SetCrashKeyValue(@"ptype", process_type);

  NSString* pid_value =
      [NSString stringWithFormat:@"%d", static_cast<unsigned int>(getpid())];
  SetCrashKeyValue(@"pid", pid_value);
}

}  // namespace breakpad

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/app/breakpad_mac.h"

#include <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>

#include "base/auto_reset.h"
#include "base/base_switches.h"
#import "base/basictypes.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/file_path.h"
#include "base/file_util.h"
#import "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#import "base/mac/scoped_nsautorelease_pool.h"
#include "base/path_service.h"
#include "base/sys_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#import "breakpad/src/client/mac/Framework/Breakpad.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/dump_without_crashing.h"
#include "chrome/common/env_vars.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/installer/util/google_update_settings.h"
#include "native_client/src/trusted/service_runtime/osx/crash_filter.h"
#include "policy/policy_constants.h"

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
  SetCrashKeyValue(base::SysUTF8ToNSString(key.as_string()),
                   base::SysUTF8ToNSString(value.as_string()));
}

void ClearCrashKeyValueImpl(const base::StringPiece& key) {
  ClearCrashKeyValue(base::SysUTF8ToNSString(key.as_string()));
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

#if !defined(DISABLE_NACL)
bool NaClBreakpadCrashFilter(int exception_type,
                             int exception_code,
                             mach_port_t crashing_thread,
                             void* context) {
  return !NaClMachThreadIsInUntrusted(crashing_thread);
}
#endif

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

  virtual void ThreadMain() OVERRIDE {
    base::PlatformThread::SetName("CrDumpHelper");
    BreakpadGenerateAndSendReport(gBreakpadRef);
  }

  DISALLOW_COPY_AND_ASSIGN(DumpHelper);
};

}  // namespace

bool IsCrashReporterEnabled() {
  return gBreakpadRef != NULL;
}

// Only called for a branded build of Chrome.app.
void InitCrashReporter() {
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
  CommandLine* command_line = CommandLine::ForCurrentProcess();

  if (is_browser) {
    // Since the configuration management infrastructure is possibly not
    // initialized when this code runs, read the policy preference directly.
    base::mac::ScopedCFTypeRef<CFStringRef> key(
        base::SysUTF8ToCFStringRef(policy::key::kMetricsReportingEnabled));
    Boolean key_valid;
    Boolean metrics_reporting_enabled = CFPreferencesGetAppBooleanValue(key,
        kCFPreferencesCurrentApplication, &key_valid);
    if (key_valid &&
        CFPreferencesAppValueIsForced(key, kCFPreferencesCurrentApplication)) {
      // Controlled by configuration manangement.
      enable_breakpad = metrics_reporting_enabled;
    } else {
      // Controlled by the user. The crash reporter may be enabled by
      // preference or through an environment variable, but the kDisableBreakpad
      // switch overrides both.
      enable_breakpad = GoogleUpdateSettings::GetCollectStatsConsent() ||
          getenv(env_vars::kHeadless) != NULL;
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

  // By setting the BREAKPAD_DUMP_LOCATION environment variable, an alternate
  // location to write brekapad crash dumps can be set.
  const char* alternate_minidump_location = getenv("BREAKPAD_DUMP_LOCATION");
  if (alternate_minidump_location) {
    base::FilePath alternate_minidump_location_path(
        alternate_minidump_location);
    if (!file_util::PathExists(alternate_minidump_location_path)) {
      LOG(ERROR) << "Directory " << alternate_minidump_location <<
          " doesn't exist";
    } else {
      PathService::Override(
          chrome::DIR_CRASH_DUMPS,
          base::FilePath(alternate_minidump_location));
      if (is_browser) {
        // Print out confirmation message to the stdout, but only print
        // from browser process so we don't flood the terminal.
        LOG(WARNING) << "Breakpad dumps will now be written in " <<
            alternate_minidump_location;
      }
    }
  }

  base::FilePath dir_crash_dumps;
  PathService::Get(chrome::DIR_CRASH_DUMPS, &dir_crash_dumps);
  [breakpad_config setObject:base::SysUTF8ToNSString(dir_crash_dumps.value())
                      forKey:@BREAKPAD_DUMP_DIRECTORY];

  // Initialize Breakpad.
  gBreakpadRef = BreakpadCreate(breakpad_config);
  if (!gBreakpadRef) {
    LOG_IF(ERROR, base::mac::AmIBundled()) << "Breakpad initializaiton failed";
    return;
  }

  // Initialize the scoped crash key system.
  base::debug::SetCrashKeyReportingFunctions(&SetCrashKeyValueImpl,
                                             &ClearCrashKeyValueImpl);
  crash_keys::RegisterChromeCrashKeys();

  // Set Breakpad metadata values.  These values are added to Info.plist during
  // the branded Google Chrome.app build.
  SetCrashKeyValue(@"ver", [info_dictionary objectForKey:@BREAKPAD_VERSION]);
  SetCrashKeyValue(@"prod", [info_dictionary objectForKey:@BREAKPAD_PRODUCT]);
  SetCrashKeyValue(@"plat", @"OS X");

  if (!is_browser) {
    // Get the guid from the command line switch.
    std::string guid =
        command_line->GetSwitchValueASCII(switches::kEnableCrashReporter);
    child_process_logging::SetClientId(guid);
  }

  logging::SetLogMessageHandler(&FatalMessageHandler);
  logging::SetDumpWithoutCrashingFunction(&DumpHelper::DumpWithoutCrashing);
}

void InitCrashProcessInfo() {
  if (gBreakpadRef == NULL) {
    return;
  }

  // Determine the process type.
  NSString* process_type = @"browser";
  std::string process_type_switch =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessType);
  if (!process_type_switch.empty()) {
    process_type = base::SysUTF8ToNSString(process_type_switch);
  }

#if !defined(DISABLE_NACL)
  if (process_type_switch == switches::kNaClLoaderProcess) {
    BreakpadSetFilterCallback(gBreakpadRef, NaClBreakpadCrashFilter, NULL);
  }
#endif

  // Store process type in crash dump.
  SetCrashKeyValue(@"ptype", process_type);
}

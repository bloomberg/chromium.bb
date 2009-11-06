// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/app/breakpad_mac.h"

#import <Foundation/Foundation.h>

#include "base/base_switches.h"
#import "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#import "base/logging.h"
#include "base/mac_util.h"
#import "base/scoped_nsautorelease_pool.h"
#include "base/sys_string_conversions.h"
#import "breakpad/src/client/mac/Framework/Breakpad.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/google_update_settings.h"

namespace {

BreakpadRef gBreakpadRef = NULL;

}  // namespace

bool IsCrashReporterEnabled() {
  return gBreakpadRef != NULL;
}

void DestructCrashReporter() {
  if (gBreakpadRef) {
    BreakpadRelease(gBreakpadRef);
    gBreakpadRef = NULL;
  }
}

// Only called for a branded build of Chrome.app.
void InitCrashReporter() {
  DCHECK(gBreakpadRef == NULL);
  base::ScopedNSAutoreleasePool autorelease_pool;

  // Check whether the user has consented to stats and crash reporting.  The
  // browser process can make this determination directly.  Helper processes
  // may not have access to the disk or to the same data as the browser
  // process, so the browser passes the consent preference to them on the
  // command line.
  NSBundle* main_bundle = mac_util::MainAppBundle();
  bool is_browser = !mac_util::IsBackgroundOnlyProcess();
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  bool enable_breakpad =
      is_browser ? GoogleUpdateSettings::GetCollectStatsConsent() :
                   command_line->HasSwitch(switches::kEnableCrashReporter);

  if (command_line->HasSwitch(switches::kDisableBreakpad)) {
    enable_breakpad = false;
  }

  if (!enable_breakpad) {
    LOG(WARNING) << "Breakpad disabled";
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
    FilePath alternate_minidump_location_path(alternate_minidump_location);
    if (!file_util::PathExists(alternate_minidump_location_path)) {
      LOG(ERROR) << "Directory " << alternate_minidump_location <<
          " doesn't exist";
    } else {
      NSFileManager* file_manager = [NSFileManager defaultManager];
      size_t minidump_location_len = strlen(alternate_minidump_location);
      DCHECK(minidump_location_len > 0);
      NSString* minidump_location = [file_manager
          stringWithFileSystemRepresentation:alternate_minidump_location
                                      length:minidump_location_len];
      [breakpad_config
          setObject:minidump_location
             forKey:@BREAKPAD_DUMP_DIRECTORY];
      if (is_browser) {
        // Print out confirmation message to the stdout, but only print
        // from browser process so we don't flood the terminal.
        LOG(WARNING) << "Breakpad dumps will now be written in " <<
            alternate_minidump_location;
      }
    }
  }

  // Initialize Breakpad.
  gBreakpadRef = BreakpadCreate(breakpad_config);
  if (!gBreakpadRef) {
    LOG(ERROR) << "Breakpad initializaiton failed";
    return;
  }

  // Set Breakpad metadata values.  These values are added to Info.plist during
  // the branded Google Chrome.app build.
  SetCrashKeyValue(@"ver", [info_dictionary objectForKey:@BREAKPAD_VERSION]);
  SetCrashKeyValue(@"prod", [info_dictionary objectForKey:@BREAKPAD_PRODUCT]);
  SetCrashKeyValue(@"plat", @"OS X");

  // Enable child process crashes to include the page URL.
  // TODO: Should this only be done for certain process types?
  child_process_logging::SetCrashKeyFunctions(SetCrashKeyValue,
                                              ClearCrashKeyValue);

  if (!is_browser) {
    // Get the guid from the command line switch.
    std::string guid = WideToASCII(
        command_line->GetSwitchValue(switches::kEnableCrashReporter));
    child_process_logging::SetClientId(guid);
   }
}

void InitCrashProcessInfo() {
  if (gBreakpadRef == NULL) {
    return;
  }

  // Determine the process type.
  NSString* process_type = @"browser";
  std::wstring process_type_switch =
      CommandLine::ForCurrentProcess()->GetSwitchValue(switches::kProcessType);
  if (!process_type_switch.empty()) {
    process_type = base::SysWideToNSString(process_type_switch);
  }

  // Store process type in crash dump.
  SetCrashKeyValue(@"ptype", process_type);
}

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

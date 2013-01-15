// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// On Mac, one can't make shortcuts with command-line arguments. Instead, we
// produce small app bundles which locate the Chromium framework and load it,
// passing the appropriate data. This is the entry point into the framework for
// those app bundles.

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/mac/app_mode_common.h"

extern "C" {

// |ChromeAppModeStart()| is the point of entry into the framework from the app
// mode loader.
__attribute__((visibility("default")))
int ChromeAppModeStart(const app_mode::ChromeAppModeInfo* info);

}  // extern "C"

int ChromeAppModeStart(const app_mode::ChromeAppModeInfo* info) {
  base::mac::ScopedNSAutoreleasePool scoped_pool;

  if (info->major_version < app_mode::kCurrentChromeAppModeInfoMajorVersion) {
    RAW_LOG(ERROR, "App Mode Loader too old.");
    return 1;
  }
  if (info->major_version > app_mode::kCurrentChromeAppModeInfoMajorVersion) {
    RAW_LOG(ERROR, "Browser Framework too old to load App Shortcut.");
    return 1;
  }

  FSRef app_fsref;
  if (!base::mac::FSRefFromPath(info->chrome_outer_bundle_path.value(),
        &app_fsref)) {
    PLOG(ERROR) << "base::mac::FSRefFromPath failed for "
        << info->chrome_outer_bundle_path.value();
    return 1;
  }
  std::string silent = std::string("--") + switches::kSilentLaunch;
  CFArrayRef launch_args =
      base::mac::NSToCFCast(@[base::SysUTF8ToNSString(silent)]);

  LSApplicationParameters ls_parameters = {
    0,     // version
    kLSLaunchDefaults,
    &app_fsref,
    NULL,  // asyncLaunchRefCon
    NULL,  // environment
    launch_args,
    NULL   // initialEvent
  };
  NSAppleEventDescriptor* initial_event =
      [NSAppleEventDescriptor
          appleEventWithEventClass:app_mode::kAEChromeAppClass
                           eventID:app_mode::kAEChromeAppLaunch
                  targetDescriptor:nil
                          returnID:kAutoGenerateReturnID
                     transactionID:kAnyTransactionID];
  NSAppleEventDescriptor* appid_descriptor = [NSAppleEventDescriptor
      descriptorWithString:base::SysUTF8ToNSString(info->app_mode_id)];
  [initial_event setParamDescriptor:appid_descriptor
                         forKeyword:keyDirectObject];
  NSAppleEventDescriptor* profile_dir_descriptor = [NSAppleEventDescriptor
      descriptorWithString:base::SysUTF8ToNSString(info->profile_dir.value())];
  [initial_event setParamDescriptor:profile_dir_descriptor
                         forKeyword:app_mode::kAEProfileDirKey];
  ls_parameters.initialEvent = const_cast<AEDesc*>([initial_event aeDesc]);
  // Send the Apple Event using launch services, launching Chrome if necessary.
  OSStatus status = LSOpenApplication(&ls_parameters, NULL);
  if (status != noErr) {
    OSSTATUS_LOG(ERROR, status) << "LSOpenApplication";
    return 1;
  }
  return 0;
}

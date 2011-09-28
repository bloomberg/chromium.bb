// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_main_mac.h"

#import <Cocoa/Cocoa.h>
#include <mach/mach.h>
#include <servers/bootstrap.h>

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/logging.h"
#import "base/mac/foundation_util.h"
#import "base/mac/scoped_nsautorelease_pool.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/policy/policy_path_parser.h"
#include "chrome/common/chrome_paths_internal.h"
#include "policy/policy_constants.h"

void CheckUserDataDirPolicy(FilePath* user_data_dir) {
  base::mac::ScopedNSAutoreleasePool pool;

  // Since the configuration management infrastructure is not initialized when
  // this code runs, read the policy preference directly.
  NSString* key = base::SysUTF8ToNSString(policy::key::kUserDataDir);
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  NSString* value = [defaults stringForKey:key];
  if (value && [defaults objectIsForcedForKey:key]) {
    std::string string_value = base::SysNSStringToUTF8(value);
    // Now replace any vars the user might have used.
    string_value =
        policy::path_parser::ExpandPathVariables(string_value);
    *user_data_dir = FilePath(string_value);
  }
}

void SetUpBundleOverrides() {
  base::mac::ScopedNSAutoreleasePool pool;

  base::mac::SetOverrideAppBundlePath(chrome::GetFrameworkBundlePath());

  NSBundle* base_bundle = chrome::OuterAppBundle();
  base::mac::SetBaseBundleID([[base_bundle bundleIdentifier] UTF8String]);
}

void SwitchToMachBootstrapSubsetPort() {
  // Testing tip: use launchctl bstree (as root) to make sure that the
  // subset port is created properly and that new mappings wind up added to
  // the subset port.

#ifndef NDEBUG
  static bool once_only = false;
  DCHECK(!once_only);
  once_only = true;
#endif

  mach_port_t self_task = mach_task_self();

  mach_port_t original_bootstrap_port;
  kern_return_t kr = task_get_bootstrap_port(self_task,
                                             &original_bootstrap_port);
  if (kr != KERN_SUCCESS) {
    LOG(ERROR) << "task_get_bootstrap_port: " << kr << " "
               << mach_error_string(kr);
    return;
  }

  mach_port_t bootstrap_subset_port;
  kr = bootstrap_subset(original_bootstrap_port,
                        self_task,
                        &bootstrap_subset_port);
  if (kr != BOOTSTRAP_SUCCESS) {
    LOG(ERROR) << "bootstrap_subset: " << kr << " " << bootstrap_strerror(kr);
    return;
  }

  kr = task_set_bootstrap_port(self_task, bootstrap_subset_port);
  if (kr != KERN_SUCCESS) {
    LOG(ERROR) << "task_set_bootstrap_port: " << kr << " "
               << mach_error_string(kr);
    return;
  }

  // Users of the bootstrap port often access it through this global variable.
  bootstrap_port = bootstrap_subset_port;
}

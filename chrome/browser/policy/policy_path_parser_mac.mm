// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_path_parser.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#import "base/mac/scoped_nsautorelease_pool.h"
#include "base/strings/sys_string_conversions.h"
#include "policy/policy_constants.h"

#import <Cocoa/Cocoa.h>
#import <SystemConfiguration/SCDynamicStore.h>
#import <SystemConfiguration/SCDynamicStoreCopySpecific.h>

#include <string>

namespace policy {

namespace path_parser {

namespace internal {

bool GetFolder(NSSearchPathDirectory id, base::FilePath::StringType* value) {
  NSArray* searchpaths =
      NSSearchPathForDirectoriesInDomains(id, NSAllDomainsMask, true);
  if ([searchpaths count] > 0) {
    NSString* variable_value = [searchpaths objectAtIndex:0];
    *value = base::SysNSStringToUTF8(variable_value);
    return true;
  }
  return false;
}

// The wrapper is used instead of callbacks since these function are
// initialized in a global table and using callbacks in the table results in
// the increase in size of static initializers.
#define WRAP_GET_FORLDER_FUNCTION(FunctionName, FolderId)   \
  bool FunctionName(base::FilePath::StringType* value) { \
    return GetFolder(FolderId, value);                      \
  }

WRAP_GET_FORLDER_FUNCTION(GetMacUserFolderPath, NSUserDirectory)
WRAP_GET_FORLDER_FUNCTION(GetMacDocumentsFolderPath, NSDocumentDirectory)

bool GetUserName(base::FilePath::StringType* value) {
  NSString* username = NSUserName();
  if (username)
    *value = base::SysNSStringToUTF8(username);
  else
    LOG(ERROR) << "Username variable can not be resolved.";
  return (username != NULL);
}

bool GetMachineName(base::FilePath::StringType* value) {
  SCDynamicStoreContext context = {0, NULL, NULL, NULL};
  SCDynamicStoreRef store = SCDynamicStoreCreate(
      kCFAllocatorDefault, CFSTR("policy_subsystem"), NULL, &context);
  CFStringRef machinename = SCDynamicStoreCopyLocalHostName(store);
  if (machinename) {
    *value = base::SysCFStringRefToUTF8(machinename);
    CFRelease(machinename);
  } else {
    LOG(ERROR) << "Machine name variable can not be resolved.";
  }
  CFRelease(store);
  return (machinename != NULL);
}

const char* kUserNamePolicyVarName = "${user_name}";
const char* kMachineNamePolicyVarName = "${machine_name}";
const char* kMacUsersDirectory = "${users}";
const char* kMacDocumentsFolderVarName = "${documents}";

// A table mapping variable names to their respective get value function
// pointers.
const VariableNameAndValueCallback kVariableNameAndValueCallbacks[] = {
    {kUserNamePolicyVarName, &GetUserName},
    {kMachineNamePolicyVarName, &GetMachineName},
    {kMacUsersDirectory, &GetMacUserFolderPath},
    {kMacDocumentsFolderVarName, &GetMacDocumentsFolderPath}};

// Total number of entries in the mapping table.
const int kNoOfVariables = arraysize(kVariableNameAndValueCallbacks);

}  // namespace internal

void CheckUserDataDirPolicy(base::FilePath* user_data_dir) {
  base::mac::ScopedNSAutoreleasePool pool;

  // Since the configuration management infrastructure is not initialized when
  // this code runs, read the policy preference directly.
  NSString* key = base::SysUTF8ToNSString(policy::key::kUserDataDir);
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  NSString* value = [defaults stringForKey:key];
  if (value && [defaults objectIsForcedForKey:key]) {
    std::string string_value = base::SysNSStringToUTF8(value);
    // Now replace any vars the user might have used.
    string_value = policy::path_parser::ExpandPathVariables(string_value);
    *user_data_dir = base::FilePath(string_value);
  }
}

}  // namespace path_parser

}  // namespace policy

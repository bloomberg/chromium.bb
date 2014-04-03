// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#include "chrome/browser/policy/policy_path_parser.h"

#include "base/bind.h"
#include "base/logging.h"

namespace policy {

namespace path_parser {

namespace internal {

const char* kMachineNamePolicyVarName = "${machine_name}";
const char* kUserNamePolicyVarName = "${user_name}";

bool GetUserName(base::FilePath::StringType* value) {
  struct passwd* user = getpwuid(geteuid());
  if (user)
    *value = base::FilePath::StringType(user->pw_name);
  else
    LOG(ERROR) << "Username variable can not be resolved. ";
  return (user != NULL);
}

bool GetMachineName(base::FilePath::StringType* value) {
  char machinename[255];
  int status;
  if ((status = gethostname(machinename, 255)) == 0)
    *value = base::FilePath::StringType(machinename);
  else
    LOG(ERROR) << "Machine name variable can not be resolved.";
  return (status == 0);
}

// A table mapping variable name to their respective getvalue callbacks.
const VariableNameAndValueCallback kVariableNameAndValueCallbacks[] = {
    {kUserNamePolicyVarName, &GetUserName},
    {kMachineNamePolicyVarName, &GetMachineName}};

// Total number of entries in the mapping table.
const int kNoOfVariables = arraysize(kVariableNameAndValueCallbacks);

}  // namespace internal

void CheckUserDataDirPolicy(base::FilePath* user_data_dir) {
  // This function is not implemented in Linux because we don't support the
  // policy on this platform.
  NOTREACHED();
}

}  // namespace path_parser

}  // namespace policy

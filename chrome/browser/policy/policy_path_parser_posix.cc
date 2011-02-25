// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <pwd.h>

#include "chrome/browser/policy/policy_path_parser.h"

#include "base/logging.h"

namespace policy {

namespace path_parser {

const char* kMachineNamePolicyVarName = "${machine_name}";
const char* kUserNamePolicyVarName = "${user_name}";

// Replaces all variable occurances in the policy string with the respective
// system settings values.
FilePath::StringType ExpandPathVariables(
    const FilePath::StringType& untranslated_string) {
  FilePath::StringType result(untranslated_string);
  // Translate two speacial variables ${user_name} and ${machine_name}
  size_t position = result.find(kUserNamePolicyVarName);
  if (position != std::string::npos) {
    struct passwd *user = getpwuid(geteuid());
    if (user) {
      result.replace(position, strlen(kUserNamePolicyVarName), user->pw_name);
    } else {
      LOG(ERROR) << "Username variable can not be resolved. ";
    }
  }
  position = result.find(kMachineNamePolicyVarName);
  if (position != std::string::npos) {
    char machinename[255];
    if (gethostname(machinename, 255) == 0) {
      result.replace(position, strlen(kMachineNamePolicyVarName), machinename);
    } else {
      LOG(ERROR) << "Machine name variable can not be resolved.";
    }
  }
  return result;
}

}  // namespace path_parser

}  // namespace policy

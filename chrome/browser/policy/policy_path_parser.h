// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_POLICY_PATH_PARSER_H_
#define CHROME_BROWSER_POLICY_POLICY_PATH_PARSER_H_

#include <string>

#include "base/file_path.h"

namespace policy {

namespace path_parser {

// This function is used to expand the variables in policy strings that
// represent paths. The set of supported variables differs between platforms
// but generally covers most standard locations that might be needed in the
// existing used cases.
// All platforms:
//   ${user_name}       - The user that is running Chrome (respects suids).
//                        (example : "johndoe")
//   ${machine_name}    - The machine name possibly with domain (example :
//                        "johnny.cg1.cooldomain.org")
// Windows only:
//   ${documents}       - The "Documents" folder for the current user.
//                        (example : "C:\Users\Administrator\Documents")
//   ${local_app_data}  - The Application Data folder for the current user.
//                        (example : "C:\Users\Administrator\AppData\Local")
//   ${roaming_app_data}- The Roamed AppData folder for the current user.
//                        (example : "C:\Users\Administrator\AppData\Roaming")
//   ${profile}         - The home folder for the current user.
//                        (example : "C:\Users\Administrator")
//   ${global_app_data} - The system-wide Application Data folder.
//                        (example : "C:\Users\All Users\AppData")
//   ${program_files}   - The "Program Files" folder for the current process.
//                        Depends on whether it is 32 or 64 bit process.
//                        (example : "C:\Program Files (x86)")
//   ${windows}         - The Windows folder
//                        (example : "C:\WINNT" or "C:\Windows")
// MacOS only:
//   ${users}           - The folder where users profiles are stored
//                        (example : "/Users")
//   ${documents}       - The "Documents" folder of the current user.
//                        (example : "/Users/johndoe/Documents")
// Any non recognized variable is not being translated at all. Variables are
// translated only once in every string because for most of these there is no
// sense in concatenating them more than once in a single path.
FilePath::StringType ExpandPathVariables(
    const FilePath::StringType& untranslated_string);

}  // namespace path_parser

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_POLICY_PATH_PARSER_H_

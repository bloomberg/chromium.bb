// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/chrome_application_mac.h"
#include "base/string_util.h"
#include "chrome/common/plugin_carbon_interpose_constants_mac.h"

void TrimInterposeEnvironment() {
  const char* interpose_list =
      getenv(plugin_interpose_strings::kDYLDInsertLibrariesKey);
  if (!interpose_list) {
    NOTREACHED() << "No interposing libraries set";
    return;
  }

  // The list is a :-separated list of paths. Because we append our interpose
  // library just before forking in plugin_process_host.cc, the only cases we
  // need to handle are:
  // 1) The whole string is "<kInterposeLibraryPath>", so just clear it, or
  // 2) ":<kInterposeLibraryPath>" is the end of the string, so trim and re-set.
  int suffix_offset = strlen(interpose_list) -
      strlen(plugin_interpose_strings::kInterposeLibraryPath);
  if (suffix_offset == 0 &&
      strcmp(interpose_list,
             plugin_interpose_strings::kInterposeLibraryPath) == 0) {
    unsetenv(plugin_interpose_strings::kDYLDInsertLibrariesKey);
  } else if (suffix_offset > 0 && interpose_list[suffix_offset - 1] == ':' &&
             strcmp(interpose_list + suffix_offset,
                    plugin_interpose_strings::kInterposeLibraryPath) == 0) {
    std::string trimmed_list =
        std::string(interpose_list).substr(0, suffix_offset - 1);
    setenv(plugin_interpose_strings::kDYLDInsertLibrariesKey,
           trimmed_list.c_str(), 1);
  } else {
    NOTREACHED() << "Missing Carbon interposing library";
  }
}

void InitializeChromeApplication() {
  [CrApplication sharedApplication];
}

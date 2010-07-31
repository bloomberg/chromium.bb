// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/chrome_application_mac.h"
#include "base/env_var.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/common/plugin_carbon_interpose_constants_mac.h"
#include "chrome/plugin/plugin_interpose_util_mac.h"

#if !defined(__LP64__)
void TrimInterposeEnvironment() {
  scoped_ptr<base::EnvVarGetter> env(base::EnvVarGetter::Create());

  std::string interpose_list;
  if (!env->GetEnv(plugin_interpose_strings::kDYLDInsertLibrariesKey,
                   &interpose_list)) {
    NOTREACHED() << "No interposing libraries set";
    return;
  }

  // The list is a :-separated list of paths. Because we append our interpose
  // library just before forking in plugin_process_host.cc, the only cases we
  // need to handle are:
  // 1) The whole string is "<kInterposeLibraryPath>", so just clear it, or
  // 2) ":<kInterposeLibraryPath>" is the end of the string, so trim and re-set.
  int suffix_offset = strlen(interpose_list.c_str()) -
      strlen(plugin_interpose_strings::kInterposeLibraryPath);

  if (suffix_offset == 0 &&
      strcmp(interpose_list.c_str(),
             plugin_interpose_strings::kInterposeLibraryPath) == 0) {
    env->UnSetEnv(plugin_interpose_strings::kDYLDInsertLibrariesKey);
  } else if (suffix_offset > 0 && interpose_list[suffix_offset - 1] == ':' &&
             strcmp(interpose_list.c_str() + suffix_offset,
                    plugin_interpose_strings::kInterposeLibraryPath) == 0) {
    std::string trimmed_list = interpose_list.substr(0, suffix_offset - 1);
    env->SetEnv(plugin_interpose_strings::kDYLDInsertLibrariesKey,
                trimmed_list.c_str());
  } else {
    NOTREACHED() << "Missing Carbon interposing library";
  }
}
#endif

void InitializeChromeApplication() {
  [CrApplication sharedApplication];

  mac_plugin_interposing::SetUpCocoaInterposing();
}

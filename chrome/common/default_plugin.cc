// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/default_plugin.h"

#include "chrome/default_plugin/plugin_main.h"
#include "webkit/plugins/npapi/plugin_list.h"

namespace chrome {

void RegisterInternalDefaultPlugin() {
  const webkit::npapi::PluginEntryPoints entry_points = {
#if !defined(OS_POSIX) || defined(OS_MACOSX)
    default_plugin::NP_GetEntryPoints,
#endif
    default_plugin::NP_Initialize,
    default_plugin::NP_Shutdown
  };

  webkit::npapi::PluginList::Singleton()->RegisterInternalPlugin(
      FilePath(webkit::npapi::kDefaultPluginLibraryName),
      "Default Plug-in",
      "Provides functionality for installing third-party plug-ins",
      "*",
      entry_points);
}

}  // namespace chrome

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/default_plugin.h"

#include "chrome/default_plugin/plugin_main.h"
#include "webkit/glue/plugins/plugin_list.h"

namespace chrome {

void RegisterInternalDefaultPlugin() {
  const NPAPI::PluginVersionInfo default_plugin = {
    FilePath(kDefaultPluginLibraryName),
    L"Default Plug-in",
    L"Provides functionality for installing third-party plug-ins",
    L"1",
    L"*",
    L"",
    L"",
    {
#if !defined(OS_POSIX) || defined(OS_MACOSX)
      default_plugin::NP_GetEntryPoints,
#endif
      default_plugin::NP_Initialize,
      default_plugin::NP_Shutdown
    }
  };

  NPAPI::PluginList::Singleton()->RegisterInternalPlugin(default_plugin);
}

}  // namespace chrome

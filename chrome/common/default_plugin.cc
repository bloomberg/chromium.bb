// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/common/default_plugin.h"
#include "chrome/default_plugin/plugin_main.h"
#include "webkit/plugins/npapi/plugin_list.h"

namespace chrome {

void RegisterInternalDefaultPlugin() {
#if defined(OS_WIN) && !defined(USE_AURA)
  // TODO(bauerb): On Windows the default plug-in can download and install
  // missing plug-ins, which we don't support in the browser yet, so keep
  // using the default plug-in on Windows until we do.
  // Aura isn't going to support NPAPI plugins.
  const webkit::npapi::PluginEntryPoints entry_points = {
#if !defined(OS_POSIX) || defined(OS_MACOSX)
    default_plugin::NP_GetEntryPoints,
#endif
    default_plugin::NP_Initialize,
    default_plugin::NP_Shutdown
  };

  webkit::WebPluginInfo info;
  info.path = FilePath(webkit::npapi::kDefaultPluginLibraryName);
  info.name = ASCIIToUTF16("Default Plug-in");
  info.version = ASCIIToUTF16("1");
  info.desc = ASCIIToUTF16("Provides functionality for installing third-party "
                           "plug-ins");

  webkit::WebPluginMimeType mimeType;
  mimeType.mime_type = "*";
  info.mime_types.push_back(mimeType);

  webkit::npapi::PluginList::Singleton()->RegisterInternalPlugin(
      info,
      entry_points,
      false);
#endif
}

}  // namespace chrome

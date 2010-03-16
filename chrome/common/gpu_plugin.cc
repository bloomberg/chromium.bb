// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/gpu_plugin.h"

#include "base/command_line.h"
#include "base/string_util.h"
#include "chrome/common/chrome_switches.h"
#include "gpu/gpu_plugin/gpu_plugin.h"
#include "webkit/glue/plugins/plugin_list.h"

#if defined(ENABLE_GPU)
#include "webkit/glue/plugins/plugin_constants_win.h"
#endif

namespace chrome {

void RegisterInternalGPUPlugin() {
#if defined(ENABLE_GPU)
  static const std::wstring kWideMimeType = ASCIIToWide(kGPUPluginMimeType);
  static const NPAPI::PluginVersionInfo kGPUPluginInfo = {
    FilePath(FILE_PATH_LITERAL("gpu-plugin")),
    L"GPU Plug-in",
    L"GPU Rendering Plug-in",
    L"1",
    kWideMimeType.c_str(),
    L"",
    L"",
    {
#if !defined(OS_POSIX) || defined(OS_MACOSX)
      gpu_plugin::NP_GetEntryPoints,
#endif
      gpu_plugin::NP_Initialize,
      gpu_plugin::NP_Shutdown
    }
  };

  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableGPUPlugin))
    NPAPI::PluginList::Singleton()->RegisterInternalPlugin(kGPUPluginInfo);
#endif  // ENABLE_GPU
}

}  // namespace chrome

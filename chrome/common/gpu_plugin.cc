// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/gpu_plugin.h"

#include "base/command_line.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_switches.h"
#include "gpu/gpu_plugin/gpu_plugin.h"
#include "webkit/plugins/npapi/plugin_list.h"

namespace chrome {

void RegisterInternalGPUPlugin() {
#if defined(ENABLE_GPU)
  static const std::wstring kWideMimeType = ASCIIToWide(
      "application/vnd.google.chrome.gpu-plugin");
  static const webkit::npapi::PluginVersionInfo kGPUPluginInfo = {
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
    webkit::npapi::PluginList::Singleton()->RegisterInternalPlugin(
        kGPUPluginInfo);
#endif  // ENABLE_GPU
}

}  // namespace chrome

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/pepper_plugin_registry.h"

#include "base/command_line.h"
#include "base/string_util.h"
#include "chrome/common/chrome_switches.h"

// static
PepperPluginRegistry* PepperPluginRegistry::GetInstance() {
  static PepperPluginRegistry registry;
  return &registry;
}

// static
void PepperPluginRegistry::GetList(std::vector<PepperPluginInfo>* plugins) {
  const std::wstring& value = CommandLine::ForCurrentProcess()->GetSwitchValue(
      switches::kRegisterPepperPlugins);
  if (value.empty())
    return;

  // FORMAT:
  // command-line = <plugin-entry> + *( LWS + "," + LWS + <plugin-entry> )
  // plugin-entry = <file-path> + *1( LWS + ";" + LWS + <mime-type> )

  std::vector<std::wstring> modules;
  SplitString(value, ',', &modules);
  for (size_t i = 0; i < modules.size(); ++i) {
    std::vector<std::wstring> parts;
    SplitString(modules[i], ';', &parts);
    if (parts.size() < 2) {
      DLOG(ERROR) << "Required mime-type not found";
      continue;
    }

    PepperPluginInfo plugin;
    plugin.path = FilePath::FromWStringHack(parts[0]);
    for (size_t j = 1; j < parts.size(); ++j)
      plugin.mime_types.push_back(WideToASCII(parts[j]));

    plugins->push_back(plugin);
  }
}

pepper::PluginModule* PepperPluginRegistry::GetModule(
    const FilePath& path) const {
  ModuleMap::const_iterator it = modules_.find(path);
  if (it == modules_.end())
    return NULL;
  return it->second;
}

PepperPluginRegistry::PepperPluginRegistry() {
  std::vector<PepperPluginInfo> plugins;
  GetList(&plugins);
  for (size_t i = 0; i < plugins.size(); ++i) {
    const FilePath& path = plugins[i].path;
    ModuleHandle module = pepper::PluginModule::CreateModule(path);
    if (!module) {
      DLOG(ERROR) << "Failed to load pepper module: " << path.value();
      continue;
    }
    modules_[path] = module;
  }
}

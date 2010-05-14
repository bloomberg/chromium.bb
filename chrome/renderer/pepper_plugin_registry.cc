// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/pepper_plugin_registry.h"

#include "base/command_line.h"
#include "base/string_util.h"
#include "chrome/common/chrome_switches.h"

// static
PepperPluginRegistry* PepperPluginRegistry::GetInstance() {
  static PepperPluginRegistry registry;
  return &registry;
}

pepper::PluginModule* PepperPluginRegistry::GetModule(
    const std::string& mime_type) const {
  ModuleMap::const_iterator it = modules_.find(mime_type);
  if (it == modules_.end())
    return NULL;
  return it->second;
}

PepperPluginRegistry::PepperPluginRegistry() {
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

    const FilePath& file_path = FilePath::FromWStringHack(parts[0]);
    ModuleHandle module = pepper::PluginModule::CreateModule(file_path);
    if (!module) {
      DLOG(ERROR) << "Failed to load pepper module: " << file_path.value();
      continue;
    }

    for (size_t j = 1; j < parts.size(); ++j) {
      const std::string& mime_type = WideToASCII(parts[j]);
      if (modules_.find(mime_type) != modules_.end()) {
        DLOG(ERROR) << "Type is already registered";
        continue;
      }
      modules_[mime_type] = module;
    }
  }
}

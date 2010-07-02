// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PEPPER_PLUGIN_REGISTRY_H_
#define CHROME_COMMON_PEPPER_PLUGIN_REGISTRY_H_

#include <string>
#include <map>
#include <vector>

#include "webkit/glue/plugins/pepper_plugin_module.h"

struct PepperPluginInfo {
  FilePath path;  // Internal plugins are of the form "internal-[name]".
  std::vector<std::string> mime_types;
  std::string name;
  std::string description;
  std::string file_extensions;
  std::string type_descriptions;
};

// This class holds references to all of the known pepper plugin modules.
class PepperPluginRegistry {
 public:
  static PepperPluginRegistry* GetInstance();

  // Returns the list of known pepper plugins.  This method is static so that
  // it can be used by the browser process, which has no need to load the
  // pepper plugin modules.
  static void GetList(std::vector<PepperPluginInfo>* plugins);

  pepper::PluginModule* GetModule(const FilePath& path) const;

 private:
  static void GetPluginInfoFromSwitch(std::vector<PepperPluginInfo>* plugins);
  static void GetExtraPlugins(std::vector<PepperPluginInfo>* plugins);

  struct InternalPluginInfo : public PepperPluginInfo {
    pepper::PluginModule::EntryPoints entry_points;
  };
  typedef std::vector<InternalPluginInfo> InternalPluginInfoList;
  static void GetInternalPluginInfo(InternalPluginInfoList* plugin_info);

  PepperPluginRegistry();

  typedef scoped_refptr<pepper::PluginModule> ModuleHandle;
  typedef std::map<FilePath, ModuleHandle> ModuleMap;
  ModuleMap modules_;
};

#endif  // CHROME_COMMON_PEPPER_PLUGIN_REGISTRY_H_

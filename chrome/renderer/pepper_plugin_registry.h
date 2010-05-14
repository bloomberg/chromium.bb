// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PEPPER_PLUGIN_REGISTRY_H_
#define CHROME_RENDERER_PEPPER_PLUGIN_REGISTRY_H_

#include <string>
#include <map>

#include "webkit/glue/plugins/pepper_plugin_module.h"

// This class holds references to all of the known pepper plugin modules.
class PepperPluginRegistry {
 public:
  static PepperPluginRegistry* GetInstance();

  pepper::PluginModule* GetModule(const std::string& mime_type) const;

 private:
  PepperPluginRegistry();

  typedef scoped_refptr<pepper::PluginModule> ModuleHandle;
  typedef std::map<std::string, ModuleHandle> ModuleMap;
  ModuleMap modules_;
};

#endif  // CHROME_RENDERER_PEPPER_PLUGIN_REGISTRY_H_

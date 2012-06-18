// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_BROWSER_PLUGIN_OLD_BROWSER_PLUGIN_REGISTRY_H_
#define CONTENT_RENDERER_BROWSER_PLUGIN_OLD_BROWSER_PLUGIN_REGISTRY_H_
#pragma once

#include "base/id_map.h"
#include "base/process.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"

namespace content {

// This class holds references to all of the known live browser plugin
// modules. There is one browser plugin module per guest renderer process.
class BrowserPluginRegistry
    : public webkit::ppapi::PluginDelegate::ModuleLifetime {
 public:
  BrowserPluginRegistry();
  virtual ~BrowserPluginRegistry();

  webkit::ppapi::PluginModule* GetModule(int guest_process_id);
  void AddModule(int guest_process_id,
                 webkit::ppapi::PluginModule* module);

  // ModuleLifetime implementation.
  virtual void PluginModuleDead(
      webkit::ppapi::PluginModule* dead_module) OVERRIDE;

 private:
  IDMap<webkit::ppapi::PluginModule> modules_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_BROWSER_PLUGIN_OLD_BROWSER_PLUGIN_REGISTRY_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/browser_plugin/old/browser_plugin_registry.h"

#include "base/logging.h"

namespace content {

BrowserPluginRegistry::BrowserPluginRegistry() {
}

BrowserPluginRegistry::~BrowserPluginRegistry() {
}

webkit::ppapi::PluginModule* BrowserPluginRegistry::GetModule(
    int guest_process_id) {
  return modules_.Lookup(guest_process_id);
}

void BrowserPluginRegistry::AddModule(int guest_process_id,
                                      webkit::ppapi::PluginModule* module) {
  modules_.AddWithID(module, guest_process_id);
}

void BrowserPluginRegistry::PluginModuleDead(
    webkit::ppapi::PluginModule* dead_module) {
  // DANGER: Don't dereference the dead_module pointer! It may be in the
  // process of being deleted.

  // Modules aren't destroyed very often and there are normally at most a
  // couple of them. So for now we just do a brute-force search.
  IDMap<webkit::ppapi::PluginModule>::iterator iter(&modules_);
  while (!iter.IsAtEnd()) {
    if (iter.GetCurrentValue() == dead_module) {
      modules_.Remove(iter.GetCurrentKey());
      return;
    }
    iter.Advance();
  }
  NOTREACHED();  // Should have always found the module above.
}

}  // namespace content

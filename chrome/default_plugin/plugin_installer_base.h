// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_DEFAULT_PLUGIN_PLUGIN_INSTALLER_BASE_H_
#define CHROME_DEFAULT_PLUGIN_PLUGIN_INSTALLER_BASE_H_

#include "base/basictypes.h"
#include "third_party/npapi/bindings/npapi.h"

// Base class for the plugin installer.
class PluginInstallerBase {
 public:
  PluginInstallerBase();
  virtual ~PluginInstallerBase();

  bool Initialize(void* module_handle, NPP instance, NPMIMEType mime_type,
                  int16 argc, char* argn[], char* argv[]);

  int renderer_process_id() const {
    return renderer_process_id_;
  }

  int render_view_id() const {
    return render_view_id_;
  }

 private:
  int renderer_process_id_;
  int render_view_id_;
};

#endif  // CHROME_DEFAULT_PLUGIN_PLUGIN_INSTALLER_BASE_H_

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/default_plugin/plugin_installer_base.h"

#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "content/common/content_constants.h"

PluginInstallerBase::PluginInstallerBase()
    : renderer_process_id_(0),
      render_view_id_(0) {
}

PluginInstallerBase::~PluginInstallerBase() {
}

bool PluginInstallerBase::Initialize(void* module_handle, NPP instance,
                                     NPMIMEType mime_type, int16 argc,
                                     char* argn[], char* argv[]) {
  for (int16_t index = 0; index < argc; ++index) {
    if (!base::strncasecmp(argn[index],
                           content::kDefaultPluginRenderProcessId,
                           strlen(argn[index]))) {
      base::StringToInt(argv[index], &renderer_process_id_);
    } else if (!base::strncasecmp(argn[index],
                                  content::kDefaultPluginRenderViewId,
                                  strlen(argn[index]))) {
      base::StringToInt(argv[index], &render_view_id_);
    }
  }
  return true;
}

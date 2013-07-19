// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/pepper_plugin_info.h"

#include "base/strings/utf_string_conversions.h"

namespace content {

PepperPluginInfo::PepperPluginInfo()
    : is_internal(false),
      is_out_of_process(false),
      is_sandboxed(true),
      permissions(0) {
}

PepperPluginInfo::~PepperPluginInfo() {
}

webkit::WebPluginInfo PepperPluginInfo::ToWebPluginInfo() const {
  webkit::WebPluginInfo info;

  info.type = is_out_of_process ?
      (is_sandboxed ?
        webkit::WebPluginInfo::PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS :
        webkit::WebPluginInfo::PLUGIN_TYPE_PEPPER_UNSANDBOXED) :
      webkit::WebPluginInfo::PLUGIN_TYPE_PEPPER_IN_PROCESS;

  info.name = name.empty() ?
      path.BaseName().LossyDisplayName() : UTF8ToUTF16(name);
  info.path = path;
  info.version = ASCIIToUTF16(version);
  info.desc = ASCIIToUTF16(description);
  info.mime_types = mime_types;
  info.pepper_permissions = permissions;

  return info;
}

}  // namespace content

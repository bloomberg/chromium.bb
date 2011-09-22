// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/user_agent.h"
#include "webkit/glue/webkit_glue.h"

#include "chrome/common/chrome_version_info.h"

class GURL;

bool IsPluginProcess() {
  return false;
}

namespace webkit_glue {

std::string BuildUserAgent(bool mimic_windows) {
  chrome::VersionInfo version_info;
  std::string product("Chrome/");
  product += version_info.is_valid() ? version_info.Version()
                                     : "0.0.0.0";
  return webkit_glue::BuildUserAgentHelper(mimic_windows, product);
}

}  // end namespace webkit_glue

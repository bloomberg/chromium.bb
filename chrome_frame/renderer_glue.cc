// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_version_info.h"

namespace webkit_glue {

void AppendToLog(const char* filename, int line, const char* message) {
}

bool IsPluginRunningInRendererProcess() {
  return true;
}

// This function is called from BuildUserAgent so we have our own version
// here instead of pulling in the whole renderer lib where this function
// is implemented for Chrome.
std::string GetProductVersion() {
  chrome::VersionInfo version_info;
  std::string product("Chrome/");
  product += version_info.is_valid() ? version_info.Version()
                                     : "0.0.0.0";
  return product;
}

}  // end namespace webkit_glue


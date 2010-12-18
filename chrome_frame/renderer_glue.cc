// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_version_info.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"

namespace webkit_glue {

// This function is called from BuildUserAgent so we have our own version
// here instead of pulling in the whole renderer lib where this function
// is implemented for Chrome.
std::string GetProductVersion() {
  scoped_ptr<FileVersionInfo> info(
      FileVersionInfo::CreateFileVersionInfoForCurrentModule());
  std::string product("Chrome/");
  product += info.get() ? WideToASCII(info->product_version()) : "0.0.0.0";
  return product;
}

}  // end namespace webkit_glue

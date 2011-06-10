// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides the Chrome-specific embedder's side of random webkit glue
// functions.

#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_version_info.h"
#include "content/common/view_messages.h"
#include "content/renderer/render_thread.h"
#include "webkit/glue/webkit_glue.h"

namespace webkit_glue {

std::string GetProductVersion() {
  chrome::VersionInfo version_info;
  std::string product("Chrome/");
  product += version_info.is_valid() ? version_info.Version()
                                     : "0.0.0.0";
  return product;
}

}  // namespace webkit_glue

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_sidebar_utils.h"

#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "googleurl/src/gurl.h"

namespace {

// Errors.
const char kInvalidPathError[] = "Invalid path: \"*\".";

}  // namespace

namespace extension_sidebar_utils {

std::string GetExtensionIdByContentId(const std::string& content_id) {
  // At the moment, content_id == extension_id.
  return content_id;
}

GURL ResolveRelativePath(const std::string& relative_path,
                         const Extension* extension,
                         std::string* error) {
  GURL url(extension->GetResourceURL(relative_path));
  if (!url.is_valid()) {
    *error = ExtensionErrorUtils::FormatErrorMessage(kInvalidPathError,
                                                     relative_path);
    return GURL();
  }
  return url;
}

}  // namespace extension_sidebar_utils

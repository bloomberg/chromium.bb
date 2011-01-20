// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_sidebar_utils.h"

#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"

namespace {

// Errors.
const char kInvalidUrlError[] = "Invalid url: \"*\".";

bool CanUseHost(const Extension* extension,
                const GURL& url,
                std::string* error) {
  if (extension->HasHostPermission(url))
    return true;

  *error = ExtensionErrorUtils::FormatErrorMessage(
      extension_manifest_errors::kCannotAccessPage, url.spec());
  return false;
}

}  // namespace

namespace extension_sidebar_utils {

std::string GetExtensionIdByContentId(const std::string& content_id) {
  // At the moment, content_id == extension_id.
  return content_id;
}

GURL ResolveAndVerifyUrl(const std::string& url_string,
                         const Extension* extension,
                         std::string* error) {
  // Resolve possibly relative URL.
  GURL url(url_string);
  if (!url.is_valid())
    url = extension->GetResourceURL(url_string);

  if (!url.is_valid()) {
    *error = ExtensionErrorUtils::FormatErrorMessage(kInvalidUrlError,
                                                     url_string);
    return GURL();
  }
  if (!url.SchemeIs(chrome::kExtensionScheme) &&
      !CanUseHost(extension, url, error)) {
    return GURL();
  }
  // Disallow requests outside of the requesting extension view's extension.
  if (url.SchemeIs(chrome::kExtensionScheme)) {
    std::string extension_id(url.host());
    if (extension_id != extension->id())
      return GURL();
  }

  return url;
}

}  // namespace extension_sidebar_utils

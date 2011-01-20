// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_SIDEBAR_UTILS_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_SIDEBAR_UTILS_H_
#pragma once

#include <string>

class Extension;
class GURL;

namespace extension_sidebar_utils {

// Returns id of an extension owning a sidebar identified by |content_id|.
std::string GetExtensionIdByContentId(const std::string& content_id);

// Resolves |url_string| relative to |extension|'s url and verifies it
// against |extension|'s host permissions.
// In case of any problem, returns an empty invalid GURL and |error| receives
// the corresponding error message.
GURL ResolveAndVerifyUrl(const std::string& url_string,
                         const Extension* extension,
                         std::string* error);

}  // namespace extension_sidebar_utils

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_SIDEBAR_UTILS_H_

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains utilities related to password manager's UI.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_UI_UTILS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_UI_UTILS_H_

#include <string>

#include "url/gurl.h"

namespace password_manager {

// Returns a string suitable for security display to the user (just like
// |FormatUrlForSecurityDisplayOmitScheme| based on |url| and |languages|) and
// without prefixes "m.", "mobile." or "www.".
std::string GetShownOrigin(const GURL& url, const std::string& languages);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_UI_UTILS_H_

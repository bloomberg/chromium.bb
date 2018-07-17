// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_WEB_APP_EXTENSION_HELPERS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_WEB_APP_EXTENSION_HELPERS_H_

#include <string>

namespace web_app {

// Compute a deterministic name based on an extension/apps's id.
std::string GenerateApplicationNameFromExtensionId(const std::string& id);

// Extracts the extension id from the app name.
std::string GetExtensionIdFromApplicationName(const std::string& app_name);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_WEB_APP_EXTENSION_HELPERS_H_

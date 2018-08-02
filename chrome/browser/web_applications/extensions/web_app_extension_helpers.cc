// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/web_app_extension_helpers.h"

#include "chrome/browser/web_applications/components/web_app_helpers.h"

namespace web_app {

std::string GenerateApplicationNameFromExtensionId(const std::string& id) {
  return GenerateApplicationNameFromAppId(id);
}

std::string GetExtensionIdFromApplicationName(const std::string& app_name) {
  return GetAppIdFromApplicationName(app_name);
}

}  // namespace web_app

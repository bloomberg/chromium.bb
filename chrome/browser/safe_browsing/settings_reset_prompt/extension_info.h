// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SETTINGS_RESET_PROMPT_EXTENSION_INFO_H_
#define CHROME_BROWSER_SAFE_BROWSING_SETTINGS_RESET_PROMPT_EXTENSION_INFO_H_

#include <string>

#include "extensions/common/extension_id.h"

namespace extensions {
class Extension;
}  // namespace extensions

namespace safe_browsing {

struct ExtensionInfo {
  explicit ExtensionInfo(const extensions::Extension* extension);
  // Convenience constructor for tests.
  ExtensionInfo(const extensions::ExtensionId& id, const std::string& name);
  ~ExtensionInfo();

  extensions::ExtensionId id;
  std::string name;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SETTINGS_RESET_PROMPT_EXTENSION_INFO_H_

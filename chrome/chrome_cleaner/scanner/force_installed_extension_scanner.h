// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_SCANNER_FORCE_INSTALLED_EXTENSION_SCANNER_H_
#define CHROME_CHROME_CLEANER_SCANNER_FORCE_INSTALLED_EXTENSION_SCANNER_H_

#include <memory>
#include <vector>

#include "chrome/chrome_cleaner/chrome_utils/force_installed_extension.h"
#include "chrome/chrome_cleaner/parsers/json_parser/json_parser_api.h"
#include "chrome/chrome_cleaner/proto/uwe_matcher.pb.h"

namespace chrome_cleaner {

class ForceInstalledExtensionScanner {
 public:
  virtual ~ForceInstalledExtensionScanner() {}
  // Create a list of Matchers from the resource ID.
  virtual std::unique_ptr<UwEMatchers> CreateUwEMatchersFromResource(
      int resource_id) = 0;

  // Gets a complete list of the force installed extensions that can then
  // be matched with known UwS.
  virtual std::vector<ForceInstalledExtension> GetForceInstalledExtensions(
      JsonParserAPI* json_parser) = 0;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_SCANNER_FORCE_INSTALLED_EXTENSION_SCANNER_H_

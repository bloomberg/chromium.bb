// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_AUTOMATION_H_
#define CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_AUTOMATION_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/common/user_script.h"

namespace extensions {

namespace api {
namespace manifest_types {
struct Automation;
}
}

class URLPatternSet;
class AutomationManifestPermission;

namespace automation_errors {
extern const char kErrorInvalidMatchPattern[];
extern const char kErrorDesktopTrueInteractFalse[];
extern const char kErrorDesktopTrueMatchesSpecified[];
extern const char kErrorURLMalformed[];
extern const char kErrorInvalidMatch[];
extern const char kErrorNoMatchesProvided[];
}

// The parsed form of the automation manifest entry.
struct AutomationInfo : public Extension::ManifestData {
 public:
  static const AutomationInfo* Get(const Extension* extension);
  static std::unique_ptr<AutomationInfo> FromValue(
      const base::Value& value,
      std::vector<InstallWarning>* install_warnings,
      base::string16* error);

  static std::unique_ptr<base::Value> ToValue(const AutomationInfo& info);
  ~AutomationInfo() override;

  // true if the extension has requested 'desktop' permission.
  const bool desktop;

  // Returns the list of hosts that this extension can request an automation
  // tree from.
  const URLPatternSet matches;

  // Whether the extension is allowed interactive access (true) or read-only
  // access (false) to the automation tree.
  const bool interact;

 private:
  AutomationInfo();
  AutomationInfo(bool desktop, const URLPatternSet& matches, bool interact);

  static std::unique_ptr<api::manifest_types::Automation> AsManifestType(
      const AutomationInfo& info);

  DISALLOW_COPY_AND_ASSIGN(AutomationInfo);
  friend class AutomationManifestPermission;
  friend class AutomationHandler;
};

// Parses the automation manifest entry.
class AutomationHandler : public ManifestHandler {
 public:
  AutomationHandler();
  ~AutomationHandler() override;

 private:
  // ManifestHandler implementation.
  bool Parse(Extension* extensions, base::string16* error) override;

  ManifestPermission* CreatePermission() override;
  ManifestPermission* CreateInitialRequiredPermission(
      const Extension* extension) override;
  const std::vector<std::string> Keys() const override;

  DISALLOW_COPY_AND_ASSIGN(AutomationHandler);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_AUTOMATION_H_

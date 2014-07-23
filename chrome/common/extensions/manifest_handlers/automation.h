// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_AUTOMATION_H_
#define CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_AUTOMATION_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/common/user_script.h"

namespace extensions {

class URLPatternSet;

namespace automation_errors {
extern const char kErrorInvalidMatchPattern[];
extern const char kErrorDesktopTrueInteractFalse[];
extern const char kErrorDesktopTrueMatchesSpecified[];
extern const char kErrorURLMalformed[];
extern const char kErrorInvalidMatch[];
extern const char kErrorNoMatchesProvided[];
}

// Parses the automation manifest entry.
class AutomationHandler : public ManifestHandler {
 public:
  AutomationHandler();
  virtual ~AutomationHandler();

  virtual bool Parse(Extension* extensions, base::string16* error) OVERRIDE;

 private:
  virtual const std::vector<std::string> Keys() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(AutomationHandler);
};

// The parsed form of the automation manifest entry.
struct AutomationInfo : public Extension::ManifestData {
 public:
  static const AutomationInfo* Get(const Extension* extension);
  static scoped_ptr<AutomationInfo> FromValue(
      const base::Value& value,
      std::vector<InstallWarning>* install_warnings,
      base::string16* error);

  virtual ~AutomationInfo();

  // true if the extension has requested 'desktop' permission.
  const bool desktop;

  // Returns the list of hosts that this extension can request an automation
  // tree from.
  const URLPatternSet matches;

  // Whether the extension is allowed interactive access (true) or read-only
  // access (false) to the automation tree.
  const bool interact;

  // Whether any matches were specified (false if automation was specified as a
  // boolean, or no matches key was provided.
  const bool specified_matches;

 private:
  AutomationInfo();
  AutomationInfo(bool desktop,
                 const URLPatternSet& matches,
                 bool interact,
                 bool specified_matches);
  DISALLOW_COPY_AND_ASSIGN(AutomationInfo);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_AUTOMATION_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_API_EXTENSION_ACTION_SCRIPT_BADGE_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_API_EXTENSION_ACTION_SCRIPT_BADGE_HANDLER_H_

#include <string>

#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/manifest_handler.h"

namespace extensions {

// Parses the "script_badge" manifest key.
class ScriptBadgeHandler : public ManifestHandler {
 public:
  ScriptBadgeHandler();
  virtual ~ScriptBadgeHandler();

  virtual bool Parse(Extension* extension, string16* error) OVERRIDE;
  virtual const std::vector<std::string>& PrerequisiteKeys() OVERRIDE;
  virtual bool AlwaysParseForType(Manifest::Type type) OVERRIDE;

 private:
  // Sets the fields of ActionInfo to the default values, matching the parent
  // extension's title and icons. Performed whether or not the script_badge key
  // is present in the manifest.
  void SetActionInfoDefaults(const Extension* extension, ActionInfo* info);

  std::vector<std::string> prerequisite_keys_;

  DISALLOW_COPY_AND_ASSIGN(ScriptBadgeHandler);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_API_EXTENSION_ACTION_SCRIPT_BADGE_HANDLER_H_

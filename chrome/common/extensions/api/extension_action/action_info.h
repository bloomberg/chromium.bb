// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_API_EXTENSION_ACTION_ACTION_INFO_H_
#define CHROME_COMMON_EXTENSIONS_API_EXTENSION_ACTION_ACTION_INFO_H_

#include <string>

#include "chrome/common/extensions/extension_icon_set.h"
#include "googleurl/src/gurl.h"

namespace extensions {

class Extension;

struct ActionInfo {
  ActionInfo();
  ~ActionInfo();

  // The types of extension actions.
  enum Type {
    TYPE_BROWSER,
    TYPE_PAGE,
    TYPE_SCRIPT_BADGE,
    TYPE_SYSTEM_INDICATOR,
  };

  // Returns the appropriate ActionInfo for the given |extension|.
  static const ActionInfo* GetScriptBadgeInfo(const Extension* extension);

  // Sets the appropriate ActionInfo as ManifestData for the given |extension|.
  // This is static since |extension| takes ownership of |info|.
  static void SetScriptBadgeInfo(Extension* extension, ActionInfo* info);

  // Empty implies the key wasn't present.
  ExtensionIconSet default_icon;
  std::string default_title;
  GURL default_popup_url;
  // action id -- only used with legacy page actions API.
  std::string id;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_API_EXTENSION_ACTION_ACTION_INFO_H_

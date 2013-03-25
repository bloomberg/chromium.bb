// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/extension_action/browser_action_handler.h"

#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/feature_switch.h"
#include "chrome/common/extensions/manifest.h"
#include "grit/generated_resources.h"

namespace extensions {

BrowserActionHandler::BrowserActionHandler() {
}

BrowserActionHandler::~BrowserActionHandler() {
}

bool BrowserActionHandler::Parse(Extension* extension,
                                 string16* error) {
  const DictionaryValue* dict = NULL;
  if (!extension->manifest()->GetDictionary(
          extension_manifest_keys::kBrowserAction, &dict)) {
    *error = ASCIIToUTF16(extension_manifest_errors::kInvalidBrowserAction);
    return false;
  }

  scoped_ptr<ActionInfo> action_info = ActionInfo::Load(extension, dict, error);
  if (!action_info.get())
    return false;  // Failed to parse browser action definition.

  ActionInfo::SetBrowserActionInfo(extension, action_info.release());

  return true;
}

bool BrowserActionHandler::Validate(
    const Extension* extension,
    std::string* error,
    std::vector<InstallWarning>* warnings) const {
  const ActionInfo* action = ActionInfo::GetBrowserActionInfo(extension);
  if (action && !action->default_icon.empty() &&
      !extension_file_util::ValidateExtensionIconSet(
          action->default_icon,
          extension,
          IDS_EXTENSION_LOAD_ICON_FOR_BROWSER_ACTION_FAILED,
          error)) {
    return false;
  }
  return true;
}

const std::vector<std::string> BrowserActionHandler::Keys() const {
  return SingleKey(extension_manifest_keys::kBrowserAction);
}

}  // namespace extensions

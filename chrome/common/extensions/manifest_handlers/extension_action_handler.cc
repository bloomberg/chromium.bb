// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_handlers/extension_action_handler.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/common/extension.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

ExtensionActionHandler::ExtensionActionHandler() {
}

ExtensionActionHandler::~ExtensionActionHandler() {
}

bool ExtensionActionHandler::Parse(Extension* extension,
                                   base::string16* error) {
  const char* key = NULL;
  const char* error_key = NULL;
  if (extension->manifest()->HasKey(manifest_keys::kPageAction)) {
    key = manifest_keys::kPageAction;
    error_key = manifest_errors::kInvalidPageAction;
  }

  if (extension->manifest()->HasKey(manifest_keys::kBrowserAction)) {
    if (key != NULL) {
      // An extension cannot have both browser and page actions.
      *error = base::ASCIIToUTF16(manifest_errors::kOneUISurfaceOnly);
      return false;
    }
    key = manifest_keys::kBrowserAction;
    error_key = manifest_errors::kInvalidBrowserAction;
  }

  if (key) {
    const base::DictionaryValue* dict = NULL;
    if (!extension->manifest()->GetDictionary(key, &dict)) {
      *error = base::ASCIIToUTF16(error_key);
      return false;
    }

    scoped_ptr<ActionInfo> action_info =
        ActionInfo::Load(extension, dict, error);
    if (!action_info)
      return false;  // Failed to parse extension action definition.

    if (key == manifest_keys::kPageAction)
      ActionInfo::SetPageActionInfo(extension, action_info.release());
    else
      ActionInfo::SetBrowserActionInfo(extension, action_info.release());
  } else {  // No key, used for synthesizing an action for extensions with none.
    if (!FeatureSwitch::extension_action_redesign()->IsEnabled())
      return true;  // Do nothing if the switch is off.
    if (Manifest::IsComponentLocation(extension->location()) ||
        Manifest::IsPolicyLocation(extension->location()))
      return true;  // Don't synthesize actions for component/policy extensions.
    if (extension->manifest()->HasKey(
            manifest_keys::kSynthesizeExtensionAction)) {
      *error = base::ASCIIToUTF16(base::StringPrintf(
          "Key %s is reserved.", manifest_keys::kSynthesizeExtensionAction));
      return false;  // No one should use this key.
    }

    // Set an empty page action. We use a page action (instead of a browser
    // action) because the action should not be seen as enabled on every page.
    ActionInfo::SetPageActionInfo(extension, new ActionInfo());
  }

  return true;
}

bool ExtensionActionHandler::Validate(
    const Extension* extension,
    std::string* error,
    std::vector<InstallWarning>* warnings) const {
  int error_message = 0;
  const ActionInfo* action = ActionInfo::GetPageActionInfo(extension);
  if (action) {
    error_message = IDS_EXTENSION_LOAD_ICON_FOR_PAGE_ACTION_FAILED;
  } else {
    action = ActionInfo::GetBrowserActionInfo(extension);
    error_message = IDS_EXTENSION_LOAD_ICON_FOR_BROWSER_ACTION_FAILED;
  }

  if (action && !action->default_icon.empty() &&
      !file_util::ValidateExtensionIconSet(
          action->default_icon, extension, error_message, error)) {
    return false;
  }
  return true;
}

bool ExtensionActionHandler::AlwaysParseForType(Manifest::Type type) const {
  return type == Manifest::TYPE_EXTENSION;
}

const std::vector<std::string> ExtensionActionHandler::Keys() const {
  std::vector<std::string> keys;
  keys.push_back(manifest_keys::kPageAction);
  keys.push_back(manifest_keys::kBrowserAction);
  keys.push_back(manifest_keys::kSynthesizeExtensionAction);
  return keys;
}

}  // namespace extensions

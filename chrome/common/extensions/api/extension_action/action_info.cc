// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/extension_action/action_info.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/extensions/api/commands/commands_handler.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handler_helpers.h"

namespace extensions {

namespace errors = manifest_errors;
namespace keys = manifest_keys;

namespace {

constexpr char kEnabled[] = "enabled";
constexpr char kDisabled[] = "disabled";

// The manifest data container for the ActionInfos for BrowserActions,
// PageActions and SystemIndicators.
struct ActionInfoData : public Extension::ManifestData {
  explicit ActionInfoData(ActionInfo* action_info);
  ~ActionInfoData() override;

  // The action associated with the BrowserAction.
  std::unique_ptr<ActionInfo> action_info;
};

ActionInfoData::ActionInfoData(ActionInfo* info) : action_info(info) {
}

ActionInfoData::~ActionInfoData() {
}

static const ActionInfo* GetActionInfo(const Extension* extension,
                                       const std::string& key) {
  ActionInfoData* data = static_cast<ActionInfoData*>(
      extension->GetManifestData(key));
  return data ? data->action_info.get() : NULL;
}

}  // namespace

ActionInfo::ActionInfo() : synthesized(false) {}

ActionInfo::ActionInfo(const ActionInfo& other) = default;

ActionInfo::~ActionInfo() {
}

// static
std::unique_ptr<ActionInfo> ActionInfo::Load(const Extension* extension,
                                             const base::DictionaryValue* dict,
                                             base::string16* error) {
  std::unique_ptr<ActionInfo> result(new ActionInfo());

  if (extension->manifest_version() == 1) {
    // kPageActionIcons is obsolete, and used by very few extensions. Continue
    // loading it, but only take the first icon as the default_icon path.
    const base::ListValue* icons = NULL;
    if (dict->HasKey(keys::kPageActionIcons) &&
        dict->GetList(keys::kPageActionIcons, &icons)) {
      base::ListValue::const_iterator iter = icons->begin();
      std::string path;
      if (iter == icons->end() || !iter->GetAsString(&path) ||
          !manifest_handler_helpers::NormalizeAndValidatePath(&path)) {
        *error = base::ASCIIToUTF16(errors::kInvalidPageActionIconPath);
        return nullptr;
      }
      // Extension icons were 19 DIP when kPageActionIcons was supported.
      result->default_icon.Add(19, path);
    }

    std::string id;
    if (dict->HasKey(keys::kPageActionId)) {
      if (!dict->GetString(keys::kPageActionId, &id)) {
        *error = base::ASCIIToUTF16(errors::kInvalidPageActionId);
        return nullptr;
      }
      result->id = id;
    }
  }

  // Read the page action |default_icon| (optional).
  // The |default_icon| value can be either dictionary {icon size -> icon path}
  // or non empty string value.
  if (dict->HasKey(keys::kPageActionDefaultIcon)) {
    const base::DictionaryValue* icons_value = NULL;
    std::string default_icon;
    if (dict->GetDictionary(keys::kPageActionDefaultIcon, &icons_value)) {
      if (!manifest_handler_helpers::LoadIconsFromDictionary(
              icons_value, &result->default_icon, error)) {
        return nullptr;
      }
    } else if (dict->GetString(keys::kPageActionDefaultIcon, &default_icon) &&
               manifest_handler_helpers::NormalizeAndValidatePath(
                   &default_icon)) {
      // Choose the most optimistic (highest) icon density regardless of the
      // actual icon resolution, whatever that happens to be. Code elsewhere
      // knows how to scale down to 19.
      result->default_icon.Add(extension_misc::EXTENSION_ICON_GIGANTOR,
                               default_icon);
    } else {
      *error = base::ASCIIToUTF16(errors::kInvalidPageActionIconPath);
      return nullptr;
    }
  }

  // Read the page action title from |default_title| if present, |name| if not
  // (both optional).
  if (dict->HasKey(keys::kPageActionDefaultTitle)) {
    if (!dict->GetString(keys::kPageActionDefaultTitle,
                         &result->default_title)) {
      *error = base::ASCIIToUTF16(errors::kInvalidPageActionDefaultTitle);
      return nullptr;
    }
  } else if (extension->manifest_version() == 1 && dict->HasKey(keys::kName)) {
    if (!dict->GetString(keys::kName, &result->default_title)) {
      *error = base::ASCIIToUTF16(errors::kInvalidPageActionName);
      return nullptr;
    }
  }

  // Read the action's |popup| (optional).
  const char* popup_key = NULL;
  if (dict->HasKey(keys::kPageActionDefaultPopup))
    popup_key = keys::kPageActionDefaultPopup;

  if (extension->manifest_version() == 1 &&
      dict->HasKey(keys::kPageActionPopup)) {
    if (popup_key) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidPageActionOldAndNewKeys,
          keys::kPageActionDefaultPopup,
          keys::kPageActionPopup);
      return nullptr;
    }
    popup_key = keys::kPageActionPopup;
  }

  if (popup_key) {
    const base::DictionaryValue* popup = NULL;
    std::string url_str;

    if (dict->GetString(popup_key, &url_str)) {
      // On success, |url_str| is set.  Nothing else to do.
    } else if (extension->manifest_version() == 1 &&
               dict->GetDictionary(popup_key, &popup)) {
      if (!popup->GetString(keys::kPageActionPopupPath, &url_str)) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidPageActionPopupPath, "<missing>");
        return nullptr;
      }
    } else {
      *error = base::ASCIIToUTF16(errors::kInvalidPageActionPopup);
      return nullptr;
    }

    if (!url_str.empty()) {
      // An empty string is treated as having no popup.
      result->default_popup_url = Extension::GetResourceURL(extension->url(),
                                                            url_str);
      if (!result->default_popup_url.is_valid()) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidPageActionPopupPath, url_str);
        return nullptr;
      }
    } else {
      DCHECK(result->default_popup_url.is_empty())
          << "Shouldn't be possible for the popup to be set.";
    }
  }

  std::string default_state;
  if (dict->HasKey(keys::kActionDefaultState)) {
    if (!dict->GetString(keys::kActionDefaultState, &default_state) ||
        !(default_state == kEnabled || default_state == kDisabled)) {
      *error = base::ASCIIToUTF16(errors::kInvalidActionDefaultState);
      return nullptr;
    }
  }

  result->default_state = default_state == kEnabled
                              ? ActionInfo::STATE_ENABLED
                              : ActionInfo::STATE_DISABLED;

  return result;
}

// static
const ActionInfo* ActionInfo::GetBrowserActionInfo(const Extension* extension) {
  return GetActionInfo(extension, keys::kBrowserAction);
}

const ActionInfo* ActionInfo::GetPageActionInfo(const Extension* extension) {
  return GetActionInfo(extension, keys::kPageAction);
}

// static
const ActionInfo* ActionInfo::GetSystemIndicatorInfo(
    const Extension* extension) {
  return GetActionInfo(extension, keys::kSystemIndicator);
}

// static
void ActionInfo::SetExtensionActionInfo(Extension* extension,
                                        ActionInfo* info) {
  extension->SetManifestData(keys::kAction,
                             std::make_unique<ActionInfoData>(info));
}

// static
void ActionInfo::SetBrowserActionInfo(Extension* extension, ActionInfo* info) {
  extension->SetManifestData(keys::kBrowserAction,
                             std::make_unique<ActionInfoData>(info));
}

// static
void ActionInfo::SetPageActionInfo(Extension* extension, ActionInfo* info) {
  extension->SetManifestData(keys::kPageAction,
                             std::make_unique<ActionInfoData>(info));
}

// static
void ActionInfo::SetSystemIndicatorInfo(Extension* extension,
                                        ActionInfo* info) {
  extension->SetManifestData(keys::kSystemIndicator,
                             std::make_unique<ActionInfoData>(info));
}

// static
bool ActionInfo::IsVerboseInstallMessage(const Extension* extension) {
  const ActionInfo* page_action_info = GetPageActionInfo(extension);
  return page_action_info &&
      (CommandsInfo::GetPageActionCommand(extension) ||
       !page_action_info->default_icon.empty());
}

}  // namespace extensions

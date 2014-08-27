// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/extension_action/action_info.h"

#include "base/memory/scoped_ptr.h"
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

// The manifest data container for the ActionInfos for BrowserActions,
// PageActions and SystemIndicators.
struct ActionInfoData : public Extension::ManifestData {
  explicit ActionInfoData(ActionInfo* action_info);
  virtual ~ActionInfoData();

  // The action associated with the BrowserAction.
  scoped_ptr<ActionInfo> action_info;
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

ActionInfo::ActionInfo() {
}

ActionInfo::~ActionInfo() {
}

// static
scoped_ptr<ActionInfo> ActionInfo::Load(const Extension* extension,
                                        const base::DictionaryValue* dict,
                                        base::string16* error) {
  scoped_ptr<ActionInfo> result(new ActionInfo());

  if (extension->manifest_version() == 1) {
    // kPageActionIcons is obsolete, and used by very few extensions. Continue
    // loading it, but only take the first icon as the default_icon path.
    const base::ListValue* icons = NULL;
    if (dict->HasKey(keys::kPageActionIcons) &&
        dict->GetList(keys::kPageActionIcons, &icons)) {
      base::ListValue::const_iterator iter = icons->begin();
      std::string path;
      if (iter == icons->end() ||
          !(*iter)->GetAsString(&path) ||
          !manifest_handler_helpers::NormalizeAndValidatePath(&path)) {
        *error = base::ASCIIToUTF16(errors::kInvalidPageActionIconPath);
        return scoped_ptr<ActionInfo>();
      }
      result->default_icon.Add(extension_misc::EXTENSION_ICON_ACTION, path);
    }

    std::string id;
    if (dict->HasKey(keys::kPageActionId)) {
      if (!dict->GetString(keys::kPageActionId, &id)) {
        *error = base::ASCIIToUTF16(errors::kInvalidPageActionId);
        return scoped_ptr<ActionInfo>();
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
      int icon_sizes[extension_misc::kNumExtensionActionIconSizes];
      for (size_t i = 0u; i < extension_misc::kNumExtensionActionIconSizes; ++i)
        icon_sizes[i] = extension_misc::kExtensionActionIconSizes[i].size;
      if (!manifest_handler_helpers::LoadIconsFromDictionary(
              icons_value,
              icon_sizes,
              extension_misc::kNumExtensionActionIconSizes,
              &result->default_icon,
              error)) {
        return scoped_ptr<ActionInfo>();
      }
    } else if (dict->GetString(keys::kPageActionDefaultIcon, &default_icon) &&
               manifest_handler_helpers::NormalizeAndValidatePath(
                   &default_icon)) {
      // Choose the most optimistic (highest) icon density - e.g. 38 not 19 -
      // regardless of the actual icon resolution, whatever that happens to be.
      // Code elsewhere knows how to scale 38 down to 19.
      result->default_icon.Add(extension_misc::EXTENSION_ICON_ACTION *
                                   extension_misc::kNumExtensionActionIconSizes,
                               default_icon);
    } else {
      *error = base::ASCIIToUTF16(errors::kInvalidPageActionIconPath);
      return scoped_ptr<ActionInfo>();
    }
  }

  // Read the page action title from |default_title| if present, |name| if not
  // (both optional).
  if (dict->HasKey(keys::kPageActionDefaultTitle)) {
    if (!dict->GetString(keys::kPageActionDefaultTitle,
                         &result->default_title)) {
      *error = base::ASCIIToUTF16(errors::kInvalidPageActionDefaultTitle);
      return scoped_ptr<ActionInfo>();
    }
  } else if (extension->manifest_version() == 1 && dict->HasKey(keys::kName)) {
    if (!dict->GetString(keys::kName, &result->default_title)) {
      *error = base::ASCIIToUTF16(errors::kInvalidPageActionName);
      return scoped_ptr<ActionInfo>();
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
      return scoped_ptr<ActionInfo>();
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
        return scoped_ptr<ActionInfo>();
      }
    } else {
      *error = base::ASCIIToUTF16(errors::kInvalidPageActionPopup);
      return scoped_ptr<ActionInfo>();
    }

    if (!url_str.empty()) {
      // An empty string is treated as having no popup.
      result->default_popup_url = Extension::GetResourceURL(extension->url(),
                                                            url_str);
      if (!result->default_popup_url.is_valid()) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidPageActionPopupPath, url_str);
        return scoped_ptr<ActionInfo>();
      }
    } else {
      DCHECK(result->default_popup_url.is_empty())
          << "Shouldn't be possible for the popup to be set.";
    }
  }

  return result.Pass();
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
void ActionInfo::SetBrowserActionInfo(Extension* extension, ActionInfo* info) {
  extension->SetManifestData(keys::kBrowserAction,
                             new ActionInfoData(info));
}

// static
void ActionInfo::SetPageActionInfo(Extension* extension, ActionInfo* info) {
  extension->SetManifestData(keys::kPageAction,
                             new ActionInfoData(info));
}

// static
void ActionInfo::SetSystemIndicatorInfo(Extension* extension,
                                        ActionInfo* info) {
  extension->SetManifestData(keys::kSystemIndicator, new ActionInfoData(info));
}

// static
bool ActionInfo::IsVerboseInstallMessage(const Extension* extension) {
  const ActionInfo* page_action_info = GetPageActionInfo(extension);
  return page_action_info &&
      (CommandsInfo::GetPageActionCommand(extension) ||
       !page_action_info->default_icon.empty());
}

}  // namespace extensions

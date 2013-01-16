// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_handler_helpers.h"

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "extensions/common/error_utils.h"

namespace errors = extension_manifest_errors;
namespace keys = extension_manifest_keys;

namespace extensions {
namespace manifest_handler_helpers {

bool NormalizeAndValidatePath(std::string* path) {
  size_t first_non_slash = path->find_first_not_of('/');
  if (first_non_slash == std::string::npos) {
    *path = "";
    return false;
  }

  *path = path->substr(first_non_slash);
  return true;
}

bool LoadIconsFromDictionary(const base::DictionaryValue* icons_value,
                             const int* icon_sizes,
                             size_t num_icon_sizes,
                             ExtensionIconSet* icons,
                             string16* error) {
  DCHECK(icons);
  for (size_t i = 0; i < num_icon_sizes; ++i) {
    std::string key = base::IntToString(icon_sizes[i]);
    if (icons_value->HasKey(key)) {
      std::string icon_path;
      if (!icons_value->GetString(key, &icon_path)) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidIconPath, key);
        return false;
      }

      if (!NormalizeAndValidatePath(&icon_path)) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidIconPath, key);
        return false;
      }

      icons->Add(icon_sizes[i], icon_path);
    }
  }
  return true;
}

scoped_ptr<ActionInfo> LoadActionInfo(
    const Extension* extension,
    const base::DictionaryValue* extension_action,
    string16* error) {
  scoped_ptr<ActionInfo> result(new ActionInfo());

  if (extension->manifest_version() == 1) {
    // kPageActionIcons is obsolete, and used by very few extensions. Continue
    // loading it, but only take the first icon as the default_icon path.
    const ListValue* icons = NULL;
    if (extension_action->HasKey(keys::kPageActionIcons) &&
        extension_action->GetList(keys::kPageActionIcons, &icons)) {
      for (ListValue::const_iterator iter = icons->begin();
           iter != icons->end(); ++iter) {
        std::string path;
        if (!(*iter)->GetAsString(&path) ||
            !manifest_handler_helpers::NormalizeAndValidatePath(&path)) {
          *error = ASCIIToUTF16(errors::kInvalidPageActionIconPath);
          return scoped_ptr<ActionInfo>();
        }

        result->default_icon.Add(extension_misc::EXTENSION_ICON_ACTION, path);
        break;
      }
    }

    std::string id;
    if (extension_action->HasKey(keys::kPageActionId)) {
      if (!extension_action->GetString(keys::kPageActionId, &id)) {
        *error = ASCIIToUTF16(errors::kInvalidPageActionId);
        return scoped_ptr<ActionInfo>();
      }
      result->id = id;
    }
  }

  // Read the page action |default_icon| (optional).
  // The |default_icon| value can be either dictionary {icon size -> icon path}
  // or non empty string value.
  if (extension_action->HasKey(keys::kPageActionDefaultIcon)) {
    const DictionaryValue* icons_value = NULL;
    std::string default_icon;
    if (extension_action->GetDictionary(keys::kPageActionDefaultIcon,
                                        &icons_value)) {
      if (!manifest_handler_helpers::LoadIconsFromDictionary(
              icons_value,
              extension_misc::kExtensionActionIconSizes,
              extension_misc::kNumExtensionActionIconSizes,
              &result->default_icon,
              error)) {
        return scoped_ptr<ActionInfo>();
      }
    } else if (extension_action->GetString(keys::kPageActionDefaultIcon,
                                           &default_icon) &&
               manifest_handler_helpers::NormalizeAndValidatePath(
                   &default_icon)) {
      result->default_icon.Add(extension_misc::EXTENSION_ICON_ACTION,
                               default_icon);
    } else {
      *error = ASCIIToUTF16(errors::kInvalidPageActionIconPath);
      return scoped_ptr<ActionInfo>();
    }
  }

  // Read the page action title from |default_title| if present, |name| if not
  // (both optional).
  if (extension_action->HasKey(keys::kPageActionDefaultTitle)) {
    if (!extension_action->GetString(keys::kPageActionDefaultTitle,
                                     &result->default_title)) {
      *error = ASCIIToUTF16(errors::kInvalidPageActionDefaultTitle);
      return scoped_ptr<ActionInfo>();
    }
  } else if (extension->manifest_version() == 1 &&
             extension_action->HasKey(keys::kName)) {
    if (!extension_action->GetString(keys::kName, &result->default_title)) {
      *error = ASCIIToUTF16(errors::kInvalidPageActionName);
      return scoped_ptr<ActionInfo>();
    }
  }

  // Read the action's |popup| (optional).
  const char* popup_key = NULL;
  if (extension_action->HasKey(keys::kPageActionDefaultPopup))
    popup_key = keys::kPageActionDefaultPopup;

  if (extension->manifest_version() == 1 &&
      extension_action->HasKey(keys::kPageActionPopup)) {
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
    const DictionaryValue* popup = NULL;
    std::string url_str;

    if (extension_action->GetString(popup_key, &url_str)) {
      // On success, |url_str| is set.  Nothing else to do.
    } else if (extension->manifest_version() == 1 &&
               extension_action->GetDictionary(popup_key, &popup)) {
      if (!popup->GetString(keys::kPageActionPopupPath, &url_str)) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidPageActionPopupPath, "<missing>");
        return scoped_ptr<ActionInfo>();
      }
    } else {
      *error = ASCIIToUTF16(errors::kInvalidPageActionPopup);
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


}  // namespace extensions
}  // namespace manifest_handler_extensions

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/input_ime/input_components_handler.h"

#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "extensions/common/error_utils.h"

namespace extensions {

InputComponentInfo::InputComponentInfo()
    : type(INPUT_COMPONENT_TYPE_NONE),
      shortcut_alt(false),
      shortcut_ctrl(false),
      shortcut_shift(false) {
}

InputComponentInfo::~InputComponentInfo() {}

InputComponents::InputComponents() {}
InputComponents::~InputComponents() {}

// static
const std::vector<InputComponentInfo>* InputComponents::GetInputComponents(
    const Extension* extension) {
  InputComponents* info = static_cast<InputComponents*>(
      extension->GetManifestData(extension_manifest_keys::kInputComponents));
  return info ? &info->input_components : NULL;
}

InputComponentsHandler::InputComponentsHandler() {
}

InputComponentsHandler::~InputComponentsHandler() {
}

bool InputComponentsHandler::Parse(const base::Value* value,
                                   Extension* extension,
                                   string16* error) {
  scoped_ptr<InputComponents> info(new InputComponents);
  const ListValue* list_value = NULL;
  if (!value->GetAsList(&list_value)) {
    *error = ASCIIToUTF16(extension_manifest_errors::kInvalidInputComponents);
    return false;
  }
  for (size_t i = 0; i < list_value->GetSize(); ++i) {
    const DictionaryValue* module_value = NULL;
    std::string name_str;
    InputComponentType type;
    std::string id_str;
    std::string description_str;
    std::string language_str;
    std::set<std::string> layouts;
    std::string shortcut_keycode_str;
    bool shortcut_alt = false;
    bool shortcut_ctrl = false;
    bool shortcut_shift = false;

    if (!list_value->GetDictionary(i, &module_value)) {
      *error = ASCIIToUTF16(extension_manifest_errors::kInvalidInputComponents);
      return false;
    }

    // Get input_components[i].name.
    if (!module_value->GetString(extension_manifest_keys::kName, &name_str)) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          extension_manifest_errors::kInvalidInputComponentName,
          base::IntToString(i));
      return false;
    }

    // Get input_components[i].type.
    std::string type_str;
    if (module_value->GetString(extension_manifest_keys::kType, &type_str)) {
      if (type_str == "ime") {
        type = INPUT_COMPONENT_TYPE_IME;
      } else {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            extension_manifest_errors::kInvalidInputComponentType,
            base::IntToString(i));
        return false;
      }
    } else {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          extension_manifest_errors::kInvalidInputComponentType,
          base::IntToString(i));
      return false;
    }

    // Get input_components[i].id.
    if (!module_value->GetString(extension_manifest_keys::kId, &id_str)) {
      id_str = "";
    }

    // Get input_components[i].description.
    if (!module_value->GetString(extension_manifest_keys::kDescription,
        &description_str)) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          extension_manifest_errors::kInvalidInputComponentDescription,
          base::IntToString(i));
      return false;
    }
    // Get input_components[i].language.
    if (!module_value->GetString(extension_manifest_keys::kLanguage,
        &language_str)) {
      language_str = "";
    }

    // Get input_components[i].layouts.
    const ListValue* layouts_value = NULL;
    if (!module_value->GetList(extension_manifest_keys::kLayouts,
        &layouts_value)) {
      *error = ASCIIToUTF16(
          extension_manifest_errors::kInvalidInputComponentLayouts);
      return false;
    }

    for (size_t j = 0; j < layouts_value->GetSize(); ++j) {
      std::string layout_name_str;
      if (!layouts_value->GetString(j, &layout_name_str)) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            extension_manifest_errors::kInvalidInputComponentLayoutName,
            base::IntToString(i), base::IntToString(j));
        return false;
      }
      layouts.insert(layout_name_str);
    }

    if (module_value->HasKey(extension_manifest_keys::kShortcutKey)) {
      const DictionaryValue* shortcut_value = NULL;
      if (!module_value->GetDictionary(extension_manifest_keys::kShortcutKey,
          &shortcut_value)) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            extension_manifest_errors::kInvalidInputComponentShortcutKey,
            base::IntToString(i));
        return false;
      }

      // Get input_components[i].shortcut_keycode.
      if (!shortcut_value->GetString(extension_manifest_keys::kKeycode,
          &shortcut_keycode_str)) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            extension_manifest_errors::kInvalidInputComponentShortcutKeycode,
            base::IntToString(i));
        return false;
      }

      // Get input_components[i].shortcut_alt.
      if (!shortcut_value->GetBoolean(extension_manifest_keys::kAltKey,
          &shortcut_alt)) {
        shortcut_alt = false;
      }

      // Get input_components[i].shortcut_ctrl.
      if (!shortcut_value->GetBoolean(extension_manifest_keys::kCtrlKey,
          &shortcut_ctrl)) {
        shortcut_ctrl = false;
      }

      // Get input_components[i].shortcut_shift.
      if (!shortcut_value->GetBoolean(extension_manifest_keys::kShiftKey,
          &shortcut_shift)) {
        shortcut_shift = false;
      }
    }

    info->input_components.push_back(InputComponentInfo());
    info->input_components.back().name = name_str;
    info->input_components.back().type = type;
    info->input_components.back().id = id_str;
    info->input_components.back().description = description_str;
    info->input_components.back().language = language_str;
    info->input_components.back().layouts.insert(layouts.begin(),
        layouts.end());
    info->input_components.back().shortcut_keycode = shortcut_keycode_str;
    info->input_components.back().shortcut_alt = shortcut_alt;
    info->input_components.back().shortcut_ctrl = shortcut_ctrl;
    info->input_components.back().shortcut_shift = shortcut_shift;
  }
  extension->SetManifestData(extension_manifest_keys::kInputComponents,
                             info.release());
  return true;
}

}  // namespace extensions

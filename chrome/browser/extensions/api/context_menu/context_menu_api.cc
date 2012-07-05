// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/context_menu/context_menu_api.h"

#include <string>

#include "base/values.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_error_utils.h"

namespace {

const char kCheckedKey[] = "checked";
const char kContextsKey[] = "contexts";
const char kDocumentUrlPatternsKey[] = "documentUrlPatterns";
const char kEnabledKey[] = "enabled";
const char kGeneratedIdKey[] = "generatedId";
const char kIdKey[] = "id";
const char kOnclickKey[] = "onclick";
const char kParentIdKey[] = "parentId";
const char kTargetUrlPatternsKey[] = "targetUrlPatterns";
const char kTitleKey[] = "title";
const char kTypeKey[] = "type";

const char kCannotFindItemError[] = "Cannot find menu item with id *";
const char kOnclickDisallowedError[] = "Extensions using event pages cannot "
    "pass an onclick parameter to chrome.contextMenus.create. Instead, use "
    "the chrome.contextMenus.onClicked event.";
const char kCheckedError[] =
    "Only items with type \"radio\" or \"checkbox\" can be checked";
const char kDuplicateIDError[] =
    "Cannot create item with duplicate id *";
const char kIdRequiredError[] = "Extensions using event pages must pass an "
    "id parameter to chrome.contextMenus.create";
const char kInvalidValueError[] = "Invalid value for *";
const char kInvalidTypeStringError[] = "Invalid type string '*'";
const char kParentsMustBeNormalError[] =
    "Parent items must have type \"normal\"";
const char kTitleNeededError[] =
    "All menu items except for separators must have a title";

std::string GetIDString(const extensions::MenuItem::Id& id) {
  if (id.uid == 0)
    return id.string_uid;
  else
    return base::IntToString(id.uid);
}

}  // namespace

namespace extensions {

bool ExtensionContextMenuFunction::ParseContexts(
    const DictionaryValue& properties,
    const char* key,
    MenuItem::ContextList* result) {
  ListValue* list = NULL;
  if (!properties.GetList(key, &list)) {
    return true;
  }
  MenuItem::ContextList tmp_result;

  std::string value;
  for (size_t i = 0; i < list->GetSize(); i++) {
    if (!list->GetString(i, &value))
      return false;

    if (value == "all") {
      tmp_result.Add(MenuItem::ALL);
    } else if (value == "page") {
      tmp_result.Add(MenuItem::PAGE);
    } else if (value == "selection") {
      tmp_result.Add(MenuItem::SELECTION);
    } else if (value == "link") {
      tmp_result.Add(MenuItem::LINK);
    } else if (value == "editable") {
      tmp_result.Add(MenuItem::EDITABLE);
    } else if (value == "image") {
      tmp_result.Add(MenuItem::IMAGE);
    } else if (value == "video") {
      tmp_result.Add(MenuItem::VIDEO);
    } else if (value == "audio") {
      tmp_result.Add(MenuItem::AUDIO);
    } else if (value == "frame") {
      tmp_result.Add(MenuItem::FRAME);
    } else {
      error_ = ExtensionErrorUtils::FormatErrorMessage(kInvalidValueError, key);
      return false;
    }
  }
  *result = tmp_result;
  return true;
}

bool ExtensionContextMenuFunction::ParseType(
    const DictionaryValue& properties,
    const MenuItem::Type& default_value,
    MenuItem::Type* result) {
  DCHECK(result);
  if (!properties.HasKey(kTypeKey)) {
    *result = default_value;
    return true;
  }

  std::string type_string;
  if (!properties.GetString(kTypeKey, &type_string))
    return false;

  if (type_string == "normal") {
    *result = MenuItem::NORMAL;
  } else if (type_string == "checkbox") {
    *result = MenuItem::CHECKBOX;
  } else if (type_string == "radio") {
    *result = MenuItem::RADIO;
  } else if (type_string == "separator") {
    *result = MenuItem::SEPARATOR;
  } else {
    error_ = ExtensionErrorUtils::FormatErrorMessage(kInvalidTypeStringError,
                                                     type_string);
    return false;
  }
  return true;
}

bool ExtensionContextMenuFunction::ParseChecked(
    MenuItem::Type type,
    const DictionaryValue& properties,
    bool default_value,
    bool* checked) {
  if (!properties.HasKey(kCheckedKey)) {
    *checked = default_value;
    return true;
  }
  if (!properties.GetBoolean(kCheckedKey, checked))
    return false;
  if (checked && type != MenuItem::CHECKBOX && type != MenuItem::RADIO) {
    error_ = kCheckedError;
    return false;
  }
  return true;
}

bool ExtensionContextMenuFunction::ParseID(const Value* value,
                                           MenuItem::Id* result) {
  return (value->GetAsInteger(&result->uid) ||
          value->GetAsString(&result->string_uid));
}

bool ExtensionContextMenuFunction::GetParent(const DictionaryValue& properties,
                                             const MenuManager& manager,
                                             MenuItem** result) {
  if (!properties.HasKey(kParentIdKey))
    return true;
  MenuItem::Id parent_id(profile()->IsOffTheRecord(), extension_id());
  Value* parent_id_value = NULL;
  if (properties.Get(kParentIdKey, &parent_id_value) &&
      !ParseID(parent_id_value, &parent_id))
    return false;

  MenuItem* parent = manager.GetItemById(parent_id);
  if (!parent) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        kCannotFindItemError, GetIDString(parent_id));
    return false;
  }
  if (parent->type() != MenuItem::NORMAL) {
    error_ = kParentsMustBeNormalError;
    return false;
  }
  *result = parent;
  return true;
}

bool CreateContextMenuFunction::RunImpl() {
  DictionaryValue* properties;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &properties));
  EXTENSION_FUNCTION_VALIDATE(properties != NULL);

  MenuItem::Id id(profile()->IsOffTheRecord(), extension_id());
  if (properties->HasKey(kIdKey)) {
    EXTENSION_FUNCTION_VALIDATE(properties->GetString(kIdKey,
                                                      &id.string_uid));
  } else {
    if (GetExtension()->has_lazy_background_page()) {
      error_ = kIdRequiredError;
      return false;
    }
    EXTENSION_FUNCTION_VALIDATE(properties->GetInteger(kGeneratedIdKey,
                                                       &id.uid));
  }

  std::string title;
  if (properties->HasKey(kTitleKey) &&
      !properties->GetString(kTitleKey, &title))
    return false;

  MenuManager* menu_manager = profile()->GetExtensionService()->menu_manager();

  if (menu_manager->GetItemById(id)) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(kDuplicateIDError,
                                                     GetIDString(id));
    return false;
  }

  if (GetExtension()->has_lazy_background_page() &&
      properties->HasKey(kOnclickKey)) {
    error_ = kOnclickDisallowedError;
    return false;
  }

  MenuItem::ContextList contexts(MenuItem::PAGE);
  if (!ParseContexts(*properties, kContextsKey, &contexts))
    return false;

  MenuItem::Type type;
  if (!ParseType(*properties, MenuItem::NORMAL, &type))
    return false;

  if (title.empty() && type != MenuItem::SEPARATOR) {
    error_ = kTitleNeededError;
    return false;
  }

  bool checked;
  if (!ParseChecked(type, *properties, false, &checked))
    return false;

  bool enabled = true;
  if (properties->HasKey(kEnabledKey))
    EXTENSION_FUNCTION_VALIDATE(properties->GetBoolean(kEnabledKey, &enabled));

  scoped_ptr<MenuItem> item(
      new MenuItem(id, title, checked, enabled, type, contexts));

  if (!item->PopulateURLPatterns(
          *properties, kDocumentUrlPatternsKey, kTargetUrlPatternsKey, &error_))
    return false;

  bool success = true;
  if (properties->HasKey(kParentIdKey)) {
    MenuItem* parent = NULL;
    if (!GetParent(*properties, *menu_manager, &parent))
      return false;
    success = menu_manager->AddChildItem(parent->id(), item.release());
  } else {
    success = menu_manager->AddContextItem(GetExtension(), item.release());
  }

  if (!success)
    return false;

  menu_manager->WriteToStorage(GetExtension());
  return true;
}

bool UpdateContextMenuFunction::RunImpl() {
  bool radioItemUpdated = false;
  MenuItem::Id item_id(profile()->IsOffTheRecord(), extension_id());
  Value* id_value = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->Get(0, &id_value));
  EXTENSION_FUNCTION_VALIDATE(ParseID(id_value, &item_id));

  ExtensionService* service = profile()->GetExtensionService();
  MenuManager* manager = service->menu_manager();
  MenuItem* item = manager->GetItemById(item_id);
  if (!item || item->extension_id() != extension_id()) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        kCannotFindItemError, GetIDString(item_id));
    return false;
  }

  DictionaryValue* properties = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &properties));
  EXTENSION_FUNCTION_VALIDATE(properties != NULL);

  // Type.
  MenuItem::Type type;
  if (!ParseType(*properties, item->type(), &type))
    return false;
  if (type != item->type()) {
    if (type == MenuItem::RADIO || item->type() == MenuItem::RADIO) {
      radioItemUpdated = true;
    }
    item->set_type(type);
  }

  // Title.
  if (properties->HasKey(kTitleKey)) {
    std::string title;
    EXTENSION_FUNCTION_VALIDATE(properties->GetString(kTitleKey, &title));
    if (title.empty() && type != MenuItem::SEPARATOR) {
      error_ = kTitleNeededError;
      return false;
    }
    item->set_title(title);
  }

  // Checked state.
  bool checked;
  if (!ParseChecked(item->type(), *properties, item->checked(), &checked))
    return false;
  if (checked != item->checked()) {
    if (!item->SetChecked(checked))
      return false;
    radioItemUpdated = true;
  }

  // Enabled.
  bool enabled;
  if (properties->HasKey(kEnabledKey)) {
    EXTENSION_FUNCTION_VALIDATE(properties->GetBoolean(kEnabledKey, &enabled));
    item->set_enabled(enabled);
  }

  // Contexts.
  MenuItem::ContextList contexts(item->contexts());
  if (!ParseContexts(*properties, kContextsKey, &contexts))
    return false;
  if (contexts != item->contexts())
    item->set_contexts(contexts);

  // Parent id.
  MenuItem* parent = NULL;
  if (!GetParent(*properties, *manager, &parent))
    return false;
  if (parent && !manager->ChangeParent(item->id(), &parent->id()))
    return false;

  if (!item->PopulateURLPatterns(
          *properties, kDocumentUrlPatternsKey, kTargetUrlPatternsKey, &error_))
    return false;

  // There is no need to call ItemUpdated if ChangeParent is called because
  // all sanitation is taken care of in ChangeParent.
  if (!parent && radioItemUpdated && !manager->ItemUpdated(item->id()))
    return false;

  manager->WriteToStorage(GetExtension());
  return true;
}

bool RemoveContextMenuFunction::RunImpl() {
  MenuItem::Id id(profile()->IsOffTheRecord(), extension_id());
  Value* id_value = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->Get(0, &id_value));
  EXTENSION_FUNCTION_VALIDATE(ParseID(id_value, &id));
  ExtensionService* service = profile()->GetExtensionService();
  MenuManager* manager = service->menu_manager();

  MenuItem* item = manager->GetItemById(id);
  // Ensure one extension can't remove another's menu items.
  if (!item || item->extension_id() != extension_id()) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        kCannotFindItemError, GetIDString(id));
    return false;
  }

  if (!manager->RemoveContextMenuItem(id))
    return false;
  manager->WriteToStorage(GetExtension());
  return true;
}

bool RemoveAllContextMenusFunction::RunImpl() {
  ExtensionService* service = profile()->GetExtensionService();
  MenuManager* manager = service->menu_manager();
  manager->RemoveAllContextItems(GetExtension()->id());
  manager->WriteToStorage(GetExtension());
  return true;
}

}  // namespace extensions

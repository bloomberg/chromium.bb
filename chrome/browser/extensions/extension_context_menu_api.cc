// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_context_menu_api.h"

#include "base/values.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"

const wchar_t kCheckedKey[] = L"checked";
const wchar_t kContextsKey[] = L"contexts";
const wchar_t kParentIdKey[] = L"parentId";
const wchar_t kTitleKey[] = L"title";
const wchar_t kTypeKey[] = L"type";

const char kTitleNeededError[] =
    "All menu items except for separators must have a title";
const char kCheckedError[] =
    "Only items with type \"radio\" or \"checkbox\" can be checked";
const char kParentsMustBeNormalError[] =
    "Parent items must have type \"normal\"";

bool ExtensionContextMenuFunction::ParseContexts(
    const DictionaryValue& properties,
    const wchar_t* key,
    ExtensionMenuItem::ContextList* result) {
  ListValue* list = NULL;
  if (!properties.GetList(key, &list)) {
    return true;
  }
  ExtensionMenuItem::ContextList tmp_result;

  std::string value;
  for (size_t i = 0; i < list->GetSize(); i++) {
    if (!list->GetString(i, &value))
      return false;

    if (value == "all") {
      tmp_result.Add(ExtensionMenuItem::ALL);
    } else if (value == "page") {
      tmp_result.Add(ExtensionMenuItem::PAGE);
    } else if (value == "selection") {
      tmp_result.Add(ExtensionMenuItem::SELECTION);
    } else if (value == "link") {
      tmp_result.Add(ExtensionMenuItem::LINK);
    } else if (value == "editable") {
      tmp_result.Add(ExtensionMenuItem::EDITABLE);
    } else if (value == "image") {
      tmp_result.Add(ExtensionMenuItem::IMAGE);
    } else if (value == "video") {
      tmp_result.Add(ExtensionMenuItem::VIDEO);
    } else if (value == "audio") {
      tmp_result.Add(ExtensionMenuItem::AUDIO);
    } else {
      error_ = "Invalid value for " + WideToASCII(key);
      return false;
    }
  }
  *result = tmp_result;
  return true;
}

bool ExtensionContextMenuFunction::ParseType(
    const DictionaryValue& properties,
    const ExtensionMenuItem::Type& default_value,
    ExtensionMenuItem::Type* result) {
  DCHECK(result);
  if (!properties.HasKey(kTypeKey)) {
    *result = default_value;
    return true;
  }

  std::string type_string;
  if (!properties.GetString(kTypeKey, &type_string))
    return false;

  if (type_string == "normal") {
    *result = ExtensionMenuItem::NORMAL;
  } else if (type_string == "checkbox") {
    *result = ExtensionMenuItem::CHECKBOX;
  } else if (type_string == "radio") {
    *result = ExtensionMenuItem::RADIO;
  } else if (type_string == "separator") {
    *result = ExtensionMenuItem::SEPARATOR;
  } else {
    error_ = "Invalid type string '" + type_string + "'";
    return false;
  }
  return true;
}

bool ExtensionContextMenuFunction::ParseChecked(
    ExtensionMenuItem::Type type,
    const DictionaryValue& properties,
    bool default_value,
    bool* checked) {
  if (!properties.HasKey(kCheckedKey)) {
    *checked = default_value;
    return true;
  }
  if (!properties.GetBoolean(kCheckedKey, checked))
    return false;
  if (checked && type != ExtensionMenuItem::CHECKBOX &&
      type != ExtensionMenuItem::RADIO) {
    error_ = "Only checkbox and radio type items can be checked";
    return false;
  }
  return true;
}

bool ExtensionContextMenuFunction::GetParent(
    const DictionaryValue& properties,
    const ExtensionMenuManager& manager,
    ExtensionMenuItem** result) {
  if (!properties.HasKey(kParentIdKey))
    return true;
  int parent_id = 0;
  if (properties.HasKey(kParentIdKey) &&
      !properties.GetInteger(kParentIdKey, &parent_id))
    return false;

  ExtensionMenuItem* parent = manager.GetItemById(parent_id);
  if (!parent) {
    error_ = "Cannot find menu item with id " + IntToString(parent_id);
    return false;
  }
  if (parent->type() != ExtensionMenuItem::NORMAL) {
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

  std::string title;
  if (properties->HasKey(kTitleKey) &&
      !properties->GetString(kTitleKey, &title))
    return false;

  ExtensionMenuManager* menu_manager =
      profile()->GetExtensionsService()->menu_manager();

  ExtensionMenuItem::ContextList contexts(ExtensionMenuItem::PAGE);
  if (!ParseContexts(*properties, kContextsKey, &contexts))
    return false;

  ExtensionMenuItem::Type type;
  if (!ParseType(*properties, ExtensionMenuItem::NORMAL, &type))
    return false;

  if (title.empty() && type != ExtensionMenuItem::SEPARATOR) {
    error_ = kTitleNeededError;
    return false;
  }

  bool checked;
  if (!ParseChecked(type, *properties, false, &checked))
    return false;

  scoped_ptr<ExtensionMenuItem> item(
      new ExtensionMenuItem(extension_id(), title, checked, type, contexts));

  int id = 0;
  if (properties->HasKey(kParentIdKey)) {
    int parent_id = 0;
    if (!properties->GetInteger(kParentIdKey, &parent_id))
      return false;
    ExtensionMenuItem* parent = menu_manager->GetItemById(parent_id);
    if (!parent) {
      error_ = "Cannot find menu item with id " + IntToString(parent_id);
      return false;
    }
    if (parent->type() != ExtensionMenuItem::NORMAL) {
      error_ = kParentsMustBeNormalError;
      return false;
    }
    id = menu_manager->AddChildItem(parent_id, item.release());
  } else {
    id = menu_manager->AddContextItem(GetExtension(), item.release());
  }

  if (id <= 0)
    return false;

  if (has_callback())
    result_.reset(Value::CreateIntegerValue(id));

  return true;
}

bool UpdateContextMenuFunction::RunImpl() {
  int item_id = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &item_id));

  ExtensionsService* service = profile()->GetExtensionsService();
  ExtensionMenuManager* manager = service->menu_manager();
  ExtensionMenuItem* item = manager->GetItemById(item_id);
  if (!item || item->extension_id() != extension_id()) {
    error_ = "Invalid menu item id: " + IntToString(item_id);
    return false;
  }

  DictionaryValue *properties = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &properties));
  EXTENSION_FUNCTION_VALIDATE(properties != NULL);

  ExtensionMenuManager* menu_manager =
      profile()->GetExtensionsService()->menu_manager();

  // Type.
  ExtensionMenuItem::Type type;
  if (!ParseType(*properties, item->type(), &type))
    return false;
  if (type != item->type())
    item->set_type(type);

  // Title.
  std::string title;
  if (properties->HasKey(kTitleKey) &&
      !properties->GetString(kTitleKey, &title))
    return false;
  if (title.empty() && type != ExtensionMenuItem::SEPARATOR) {
    error_ = kTitleNeededError;
    return false;
  }
  item->set_title(title);

  // Checked state.
  bool checked;
  if (!ParseChecked(item->type(), *properties, item->checked(), &checked))
    return false;
  if (checked != item->checked()) {
    if (!item->SetChecked(checked))
      return false;
  }

  // Contexts.
  ExtensionMenuItem::ContextList contexts(item->contexts());
  if (!ParseContexts(*properties, kContextsKey, &contexts))
    return false;
  if (contexts != item->contexts())
    item->set_contexts(contexts);

  // Parent id.
  ExtensionMenuItem* parent = NULL;
  if (!GetParent(*properties, *menu_manager, &parent))
    return false;
  if (parent && !menu_manager->ChangeParent(item->id(), parent->id()))
    return false;

  return true;
}

bool RemoveContextMenuFunction::RunImpl() {
  int id = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &id));
  ExtensionsService* service = profile()->GetExtensionsService();
  ExtensionMenuManager* manager = service->menu_manager();

  ExtensionMenuItem* item = manager->GetItemById(id);
  // Ensure one extension can't remove another's menu items.
  if (!item || item->extension_id() != extension_id()) {
    error_ = StringPrintf("no menu item with id %d is registered", id);
    return false;
  }

  return manager->RemoveContextMenuItem(id);
}

bool RemoveAllContextMenusFunction::RunImpl() {
  ExtensionsService* service = profile()->GetExtensionsService();
  ExtensionMenuManager* manager = service->menu_manager();
  manager->RemoveAllContextItems(extension_id());
  return true;
}

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_context_menu_api.h"

#include "chrome/browser/extensions/extension_menu_manager.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"

const wchar_t kEnabledContextsKey[] = L"enabledContexts";
const wchar_t kCheckedKey[] = L"checked";
const wchar_t kContextsKey[] = L"contexts";
const wchar_t kParentIdKey[] = L"parentId";
const wchar_t kTitleKey[] = L"title";
const wchar_t kTypeKey[] = L"type";

const char kTitleNeededError[] =
    "All menu items except for separators must have a title";
const char kCheckedError[] =
    "Only items with type RADIO or CHECKBOX can be checked";
const char kParentsMustBeNormalError[] =
    "Parent items must have type NORMAL";

bool ParseContexts(const DictionaryValue* properties,
                   const std::wstring& key,
                   ExtensionMenuItem::ContextList* result) {
  ListValue* list = NULL;
  if (!properties->GetList(key, &list)) {
    return true;
  }
  ExtensionMenuItem::ContextList tmp_result;

  std::string value;
  for (size_t i = 0; i < list->GetSize(); i++) {
    if (!list->GetString(i, &value))
      return false;

    if (value == "ALL")
      tmp_result.Add(ExtensionMenuItem::ALL);
    else if (value == "PAGE")
      tmp_result.Add(ExtensionMenuItem::PAGE);
    else if (value == "SELECTION")
      tmp_result.Add(ExtensionMenuItem::SELECTION);
    else if (value == "LINK")
      tmp_result.Add(ExtensionMenuItem::LINK);
    else if (value == "EDITABLE")
      tmp_result.Add(ExtensionMenuItem::EDITABLE);
    else if (value == "IMAGE")
      tmp_result.Add(ExtensionMenuItem::IMAGE);
    else if (value == "VIDEO")
      tmp_result.Add(ExtensionMenuItem::VIDEO);
    else if (value == "AUDIO")
      tmp_result.Add(ExtensionMenuItem::AUDIO);
    else
      return false;
  }
  *result = tmp_result;
  return true;
}

bool CreateContextMenuFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_DICTIONARY));
  const DictionaryValue* properties = args_as_dictionary();
  std::string title;
  if (properties->HasKey(kTitleKey))
    EXTENSION_FUNCTION_VALIDATE(properties->GetString(kTitleKey, &title));

  ExtensionMenuManager* menu_manager =
      profile()->GetExtensionsService()->menu_manager();

  ExtensionMenuItem::ContextList contexts(ExtensionMenuItem::PAGE);
  if (!ParseContexts(properties, kContextsKey, &contexts))
    EXTENSION_FUNCTION_ERROR("Invalid value for " + WideToASCII(kContextsKey));

  ExtensionMenuItem::ContextList enabled_contexts = contexts;
  if (!ParseContexts(properties, kEnabledContextsKey, &enabled_contexts))
    EXTENSION_FUNCTION_ERROR("Invalid value for " +
                             WideToASCII(kEnabledContextsKey));

  ExtensionMenuItem::Type type = ExtensionMenuItem::NORMAL;
  if (properties->HasKey(kTypeKey)) {
    std::string type_string;
    EXTENSION_FUNCTION_VALIDATE(properties->GetString(kTypeKey, &type_string));
    if (type_string == "CHECKBOX")
      type = ExtensionMenuItem::CHECKBOX;
    else if (type_string == "RADIO")
      type = ExtensionMenuItem::RADIO;
    else if (type_string == "SEPARATOR")
      type = ExtensionMenuItem::SEPARATOR;
    else if (type_string != "NORMAL")
      EXTENSION_FUNCTION_ERROR("Invalid type string '" + type_string + "'");
  }

  if (title.empty() && type != ExtensionMenuItem::SEPARATOR)
    EXTENSION_FUNCTION_ERROR(kTitleNeededError);

  bool checked = false;
  if (properties->HasKey(kCheckedKey)) {
    EXTENSION_FUNCTION_VALIDATE(properties->GetBoolean(kCheckedKey, &checked));
    if (checked && type != ExtensionMenuItem::CHECKBOX &&
        type != ExtensionMenuItem::RADIO)
      EXTENSION_FUNCTION_ERROR(kCheckedError);
  }

  scoped_ptr<ExtensionMenuItem> item(
      new ExtensionMenuItem(extension_id(), title, checked, type, contexts,
                            enabled_contexts));

  int id = 0;
  if (properties->HasKey(kParentIdKey)) {
    int parent_id = 0;
    EXTENSION_FUNCTION_VALIDATE(properties->GetInteger(kParentIdKey,
                                                       &parent_id));
    ExtensionMenuItem* parent = menu_manager->GetItemById(parent_id);
    if (!parent)
      EXTENSION_FUNCTION_ERROR("Cannot find menu item with id " +
                               IntToString(parent_id));
    if (parent->type() != ExtensionMenuItem::NORMAL)
      EXTENSION_FUNCTION_ERROR(kParentsMustBeNormalError);

    id = menu_manager->AddChildItem(parent_id, item.release());
  } else {
    id = menu_manager->AddContextItem(item.release());
  }
  EXTENSION_FUNCTION_VALIDATE(id > 0);

  if (has_callback())
    result_.reset(Value::CreateIntegerValue(id));

  return true;
}

bool RemoveContextMenuFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_INTEGER));
  int id = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetAsInteger(&id));
  ExtensionsService* service = profile()->GetExtensionsService();
  ExtensionMenuManager* manager = service->menu_manager();

  ExtensionMenuItem* item = manager->GetItemById(id);
  // Ensure one extension can't remove another's menu items.
  if (!item || item->extension_id() != extension_id())
    EXTENSION_FUNCTION_ERROR(
        StringPrintf("no menu item with id %d is registered", id));

  EXTENSION_FUNCTION_VALIDATE(manager->RemoveContextMenuItem(id));
  return true;
}

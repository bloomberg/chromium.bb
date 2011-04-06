// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_context_menu_api.h"

#include <string>

#include "base/values.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_error_utils.h"

const char kCheckedKey[] = "checked";
const char kContextsKey[] = "contexts";
const char kDocumentUrlPatternsKey[] = "documentUrlPatterns";
const char kGeneratedIdKey[] = "generatedId";
const char kParentIdKey[] = "parentId";
const char kTargetUrlPatternsKey[] = "targetUrlPatterns";
const char kTitleKey[] = "title";
const char kTypeKey[] = "type";

const char kCannotFindItemError[] = "Cannot find menu item with id *";
const char kCheckedError[] =
    "Only items with type \"radio\" or \"checkbox\" can be checked";
const char kInvalidURLPatternError[] = "Invalid url pattern '*'";
const char kInvalidValueError[] = "Invalid value for *";
const char kInvalidTypeStringError[] = "Invalid type string '*'";
const char kParentsMustBeNormalError[] =
    "Parent items must have type \"normal\"";
const char kTitleNeededError[] =
    "All menu items except for separators must have a title";


bool ExtensionContextMenuFunction::ParseContexts(
    const DictionaryValue& properties,
    const char* key,
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
    } else if (value == "frame") {
      tmp_result.Add(ExtensionMenuItem::FRAME);
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
    error_ = ExtensionErrorUtils::FormatErrorMessage(kInvalidTypeStringError,
                                                     type_string);
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
    error_ = kCheckedError;
    return false;
  }
  return true;
}

bool ExtensionContextMenuFunction::ParseURLPatterns(
    const DictionaryValue& properties,
    const char* key,
    ExtensionExtent* result) {
  if (!properties.HasKey(key))
    return true;
  ListValue* list = NULL;
  if (!properties.GetList(key, &list))
    return false;
  for (ListValue::iterator i = list->begin(); i != list->end(); ++i) {
    std::string tmp;
    if (!(*i)->GetAsString(&tmp))
      return false;

    URLPattern pattern(ExtensionMenuManager::kAllowedSchemes);
    // TODO(skerner):  Consider enabling strict pattern parsing
    // if this extension's location indicates that it is under development.
    if (URLPattern::PARSE_SUCCESS != pattern.Parse(tmp,
                                                   URLPattern::PARSE_LENIENT)) {
      error_ = ExtensionErrorUtils::FormatErrorMessage(kInvalidURLPatternError,
                                                       tmp);
      return false;
    }
    result->AddPattern(pattern);
  }
  return true;
}

bool ExtensionContextMenuFunction::SetURLPatterns(
    const DictionaryValue& properties,
    ExtensionMenuItem* item) {
  // Process the documentUrlPattern value.
  ExtensionExtent document_url_patterns;
  if (!ParseURLPatterns(properties, kDocumentUrlPatternsKey,
                        &document_url_patterns))
    return false;

  if (!document_url_patterns.is_empty()) {
    item->set_document_url_patterns(document_url_patterns);
  }

  // Process the targetUrlPattern value.
  ExtensionExtent target_url_patterns;
  if (!ParseURLPatterns(properties, kTargetUrlPatternsKey,
                        &target_url_patterns))
    return false;

  if (!target_url_patterns.is_empty()) {
    item->set_target_url_patterns(target_url_patterns);
  }

  return true;
}

bool ExtensionContextMenuFunction::GetParent(
    const DictionaryValue& properties,
    const ExtensionMenuManager& manager,
    ExtensionMenuItem** result) {
  if (!properties.HasKey(kParentIdKey))
    return true;
  ExtensionMenuItem::Id parent_id(profile(), extension_id(), 0);
  if (properties.HasKey(kParentIdKey) &&
      !properties.GetInteger(kParentIdKey, &parent_id.uid))
    return false;

  ExtensionMenuItem* parent = manager.GetItemById(parent_id);
  if (!parent) {
    error_ = "Cannot find menu item with id " +
        base::IntToString(parent_id.uid);
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

  ExtensionMenuItem::Id id(profile(), extension_id(), 0);
  EXTENSION_FUNCTION_VALIDATE(properties->GetInteger(kGeneratedIdKey,
                                                     &id.uid));
  std::string title;
  if (properties->HasKey(kTitleKey) &&
      !properties->GetString(kTitleKey, &title))
    return false;

  ExtensionMenuManager* menu_manager =
      profile()->GetExtensionService()->menu_manager();

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
      new ExtensionMenuItem(id, title, checked, type, contexts));

  if (!SetURLPatterns(*properties, item.get()))
    return false;

  bool success = true;
  if (properties->HasKey(kParentIdKey)) {
    ExtensionMenuItem::Id parent_id(profile(), extension_id(), 0);
    EXTENSION_FUNCTION_VALIDATE(properties->GetInteger(kParentIdKey,
                                                       &parent_id.uid));
    ExtensionMenuItem* parent = menu_manager->GetItemById(parent_id);
    if (!parent) {
      error_ = ExtensionErrorUtils::FormatErrorMessage(
          kCannotFindItemError, base::IntToString(parent_id.uid));
      return false;
    }
    if (parent->type() != ExtensionMenuItem::NORMAL) {
      error_ = kParentsMustBeNormalError;
      return false;
    }
    success = menu_manager->AddChildItem(parent_id, item.release());
  } else {
    success = menu_manager->AddContextItem(GetExtension(), item.release());
  }

  if (!success)
    return false;

  return true;
}

bool UpdateContextMenuFunction::RunImpl() {
  ExtensionMenuItem::Id item_id(profile(), extension_id(), 0);
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &item_id.uid));

  ExtensionService* service = profile()->GetExtensionService();
  ExtensionMenuManager* manager = service->menu_manager();
  ExtensionMenuItem* item = manager->GetItemById(item_id);
  if (!item || item->extension_id() != extension_id()) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        kCannotFindItemError, base::IntToString(item_id.uid));
    return false;
  }

  DictionaryValue *properties = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &properties));
  EXTENSION_FUNCTION_VALIDATE(properties != NULL);

  ExtensionMenuManager* menu_manager =
      profile()->GetExtensionService()->menu_manager();

  // Type.
  ExtensionMenuItem::Type type;
  if (!ParseType(*properties, item->type(), &type))
    return false;
  if (type != item->type())
    item->set_type(type);

  // Title.
  if (properties->HasKey(kTitleKey)) {
    std::string title;
    EXTENSION_FUNCTION_VALIDATE(properties->GetString(kTitleKey, &title));
    if (title.empty() && type != ExtensionMenuItem::SEPARATOR) {
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
  if (parent && !menu_manager->ChangeParent(item->id(), &parent->id()))
    return false;

  if (!SetURLPatterns(*properties, item))
    return false;

  return true;
}

bool RemoveContextMenuFunction::RunImpl() {
  ExtensionMenuItem::Id id(profile(), extension_id(), 0);
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &id.uid));
  ExtensionService* service = profile()->GetExtensionService();
  ExtensionMenuManager* manager = service->menu_manager();

  ExtensionMenuItem* item = manager->GetItemById(id);
  // Ensure one extension can't remove another's menu items.
  if (!item || item->extension_id() != extension_id()) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        kCannotFindItemError, base::IntToString(id.uid));
    return false;
  }

  return manager->RemoveContextMenuItem(id);
}

bool RemoveAllContextMenusFunction::RunImpl() {
  ExtensionService* service = profile()->GetExtensionService();
  ExtensionMenuManager* manager = service->menu_manager();
  manager->RemoveAllContextItems(extension_id());
  return true;
}

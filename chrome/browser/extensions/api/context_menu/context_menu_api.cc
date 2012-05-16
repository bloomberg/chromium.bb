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

const char kCheckedKey[] = "checked";
const char kContextsKey[] = "contexts";
const char kDocumentUrlPatternsKey[] = "documentUrlPatterns";
const char kEnabledKey[] = "enabled";
const char kGeneratedIdKey[] = "generatedId";
const char kIdKey[] = "id";
const char kParentIdKey[] = "parentId";
const char kTargetUrlPatternsKey[] = "targetUrlPatterns";
const char kTitleKey[] = "title";
const char kTypeKey[] = "type";

const char kCannotFindItemError[] = "Cannot find menu item with id *";
const char kCheckedError[] =
    "Only items with type \"radio\" or \"checkbox\" can be checked";
const char kDuplicateIDError[] =
    "Cannot create item with duplicate id *";
const char kInvalidURLPatternError[] = "Invalid url pattern '*'";
const char kInvalidValueError[] = "Invalid value for *";
const char kInvalidTypeStringError[] = "Invalid type string '*'";
const char kParentsMustBeNormalError[] =
    "Parent items must have type \"normal\"";
const char kTitleNeededError[] =
    "All menu items except for separators must have a title";

namespace extensions {

namespace {

std::string GetIDString(const ExtensionMenuItem::Id& id) {
  if (id.uid == 0)
    return id.string_uid;
  else
    return base::IntToString(id.uid);
}

}  // namespace

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
    URLPatternSet* result) {
  if (!properties.HasKey(key))
    return true;
  ListValue* list = NULL;
  if (!properties.GetList(key, &list))
    return false;
  for (ListValue::iterator i = list->begin(); i != list->end(); ++i) {
    std::string tmp;
    if (!(*i)->GetAsString(&tmp))
      return false;

    URLPattern pattern(URLPattern::SCHEME_ALL);
    // TODO(skerner):  Consider enabling strict pattern parsing
    // if this extension's location indicates that it is under development.
    if (URLPattern::PARSE_SUCCESS != pattern.Parse(tmp)) {
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
  URLPatternSet document_url_patterns;
  if (!ParseURLPatterns(properties, kDocumentUrlPatternsKey,
                        &document_url_patterns))
    return false;

  if (!document_url_patterns.is_empty()) {
    item->set_document_url_patterns(document_url_patterns);
  }

  // Process the targetUrlPattern value.
  URLPatternSet target_url_patterns;
  if (!ParseURLPatterns(properties, kTargetUrlPatternsKey,
                        &target_url_patterns))
    return false;

  if (!target_url_patterns.is_empty()) {
    item->set_target_url_patterns(target_url_patterns);
  }

  return true;
}

bool ExtensionContextMenuFunction::ParseID(
    const Value* value,
    ExtensionMenuItem::Id* result) {
  if (value->GetAsInteger(&result->uid) ||
      value->GetAsString(&result->string_uid)) {
    return true;
  } else {
    return false;
  }
}

bool ExtensionContextMenuFunction::GetParent(
    const DictionaryValue& properties,
    const ExtensionMenuManager& manager,
    ExtensionMenuItem** result) {
  if (!properties.HasKey(kParentIdKey))
    return true;
  ExtensionMenuItem::Id parent_id(profile(), extension_id());
  Value* parent_id_value = NULL;
  if (properties.Get(kParentIdKey, &parent_id_value) &&
      !ParseID(parent_id_value, &parent_id))
    return false;

  ExtensionMenuItem* parent = manager.GetItemById(parent_id);
  if (!parent) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        kCannotFindItemError, GetIDString(parent_id));
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

  ExtensionMenuItem::Id id(profile(), extension_id());
  if (properties->HasKey(kIdKey)) {
    EXTENSION_FUNCTION_VALIDATE(properties->GetString(kIdKey,
                                                      &id.string_uid));
  } else {
    EXTENSION_FUNCTION_VALIDATE(properties->GetInteger(kGeneratedIdKey,
                                                       &id.uid));
  }

  std::string title;
  if (properties->HasKey(kTitleKey) &&
      !properties->GetString(kTitleKey, &title))
    return false;

  ExtensionMenuManager* menu_manager =
      profile()->GetExtensionService()->menu_manager();

  if (menu_manager->GetItemById(id)) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(kDuplicateIDError,
                                                     GetIDString(id));
    return false;
  }

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

  bool enabled = true;
  if (properties->HasKey(kEnabledKey))
    EXTENSION_FUNCTION_VALIDATE(properties->GetBoolean(kEnabledKey, &enabled));

  scoped_ptr<ExtensionMenuItem> item(
      new ExtensionMenuItem(id, title, checked, enabled, type, contexts));

  if (!SetURLPatterns(*properties, item.get()))
    return false;

  bool success = true;
  if (properties->HasKey(kParentIdKey)) {
    ExtensionMenuItem* parent = NULL;
    if (!GetParent(*properties, *menu_manager, &parent))
      return false;
    success = menu_manager->AddChildItem(parent->id(), item.release());
  } else {
    success = menu_manager->AddContextItem(GetExtension(), item.release());
  }

  if (!success)
    return false;

  return true;
}

bool UpdateContextMenuFunction::RunImpl() {
  bool radioItemUpdated = false;
  ExtensionMenuItem::Id item_id(profile(), extension_id());
  Value* id_value = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->Get(0, &id_value));
  EXTENSION_FUNCTION_VALIDATE(ParseID(id_value, &item_id));

  ExtensionService* service = profile()->GetExtensionService();
  ExtensionMenuManager* manager = service->menu_manager();
  ExtensionMenuItem* item = manager->GetItemById(item_id);
  if (!item || item->extension_id() != extension_id()) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        kCannotFindItemError, GetIDString(item_id));
    return false;
  }

  DictionaryValue* properties = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &properties));
  EXTENSION_FUNCTION_VALIDATE(properties != NULL);

  // Type.
  ExtensionMenuItem::Type type;
  if (!ParseType(*properties, item->type(), &type))
    return false;
  if (type != item->type()) {
    if (type == ExtensionMenuItem::RADIO ||
        item->type() == ExtensionMenuItem::RADIO) {
      radioItemUpdated = true;
    }
    item->set_type(type);
  }

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
    radioItemUpdated = true;
  }

  // Enabled.
  bool enabled;
  if (properties->HasKey(kEnabledKey)) {
    EXTENSION_FUNCTION_VALIDATE(properties->GetBoolean(kEnabledKey, &enabled));
    item->set_enabled(enabled);
  }

  // Contexts.
  ExtensionMenuItem::ContextList contexts(item->contexts());
  if (!ParseContexts(*properties, kContextsKey, &contexts))
    return false;
  if (contexts != item->contexts())
    item->set_contexts(contexts);

  // Parent id.
  ExtensionMenuItem* parent = NULL;
  if (!GetParent(*properties, *manager, &parent))
    return false;
  if (parent && !manager->ChangeParent(item->id(), &parent->id()))
    return false;

  if (!SetURLPatterns(*properties, item))
    return false;

  // There is no need to call ItemUpdated if ChangeParent is called because
  // all sanitation is taken care of in ChangeParent.
  if (!parent && radioItemUpdated && !manager->ItemUpdated(item->id()))
    return false;

  return true;
}

bool RemoveContextMenuFunction::RunImpl() {
  ExtensionMenuItem::Id id(profile(), extension_id());
  Value* id_value = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->Get(0, &id_value));
  EXTENSION_FUNCTION_VALIDATE(ParseID(id_value, &id));
  ExtensionService* service = profile()->GetExtensionService();
  ExtensionMenuManager* manager = service->menu_manager();

  ExtensionMenuItem* item = manager->GetItemById(id);
  // Ensure one extension can't remove another's menu items.
  if (!item || item->extension_id() != extension_id()) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        kCannotFindItemError, GetIDString(id));
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

}  // namespace extensions

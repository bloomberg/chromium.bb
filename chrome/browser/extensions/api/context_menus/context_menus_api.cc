// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/context_menus/context_menus_api.h"

#include <string>

#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/context_menus.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/url_pattern_set.h"

using extensions::ErrorUtils;

namespace {

const char kGeneratedIdKey[] = "generatedId";

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
const char kParentsMustBeNormalError[] =
    "Parent items must have type \"normal\"";
const char kTitleNeededError[] =
    "All menu items except for separators must have a title";
const char kLauncherNotAllowedError[] =
    "Only packaged apps are allowed to use 'launcher' context";

std::string GetIDString(const extensions::MenuItem::Id& id) {
  if (id.uid == 0)
    return id.string_uid;
  else
    return base::IntToString(id.uid);
}

template<typename PropertyWithEnumT>
extensions::MenuItem::ContextList GetContexts(
    const PropertyWithEnumT& property) {
  extensions::MenuItem::ContextList contexts;
  for (size_t i = 0; i < property.contexts->size(); ++i) {
    switch (property.contexts->at(i)) {
      case PropertyWithEnumT::CONTEXTS_TYPE_ALL:
        contexts.Add(extensions::MenuItem::ALL);
        break;
      case PropertyWithEnumT::CONTEXTS_TYPE_PAGE:
        contexts.Add(extensions::MenuItem::PAGE);
        break;
      case PropertyWithEnumT::CONTEXTS_TYPE_SELECTION:
        contexts.Add(extensions::MenuItem::SELECTION);
        break;
      case PropertyWithEnumT::CONTEXTS_TYPE_LINK:
        contexts.Add(extensions::MenuItem::LINK);
        break;
      case PropertyWithEnumT::CONTEXTS_TYPE_EDITABLE:
        contexts.Add(extensions::MenuItem::EDITABLE);
        break;
      case PropertyWithEnumT::CONTEXTS_TYPE_IMAGE:
        contexts.Add(extensions::MenuItem::IMAGE);
        break;
      case PropertyWithEnumT::CONTEXTS_TYPE_VIDEO:
        contexts.Add(extensions::MenuItem::VIDEO);
        break;
      case PropertyWithEnumT::CONTEXTS_TYPE_AUDIO:
        contexts.Add(extensions::MenuItem::AUDIO);
        break;
      case PropertyWithEnumT::CONTEXTS_TYPE_FRAME:
        contexts.Add(extensions::MenuItem::FRAME);
        break;
      case PropertyWithEnumT::CONTEXTS_TYPE_LAUNCHER:
        contexts.Add(extensions::MenuItem::LAUNCHER);
        break;
      case PropertyWithEnumT::CONTEXTS_TYPE_NONE:
        NOTREACHED();
    }
  }
  return contexts;
}

template<typename PropertyWithEnumT>
extensions::MenuItem::Type GetType(const PropertyWithEnumT& property) {
  switch (property.type) {
    case PropertyWithEnumT::TYPE_NONE:
    case PropertyWithEnumT::TYPE_NORMAL:
      return extensions::MenuItem::NORMAL;
    case PropertyWithEnumT::TYPE_CHECKBOX:
      return extensions::MenuItem::CHECKBOX;
    case PropertyWithEnumT::TYPE_RADIO:
      return extensions::MenuItem::RADIO;
    case PropertyWithEnumT::TYPE_SEPARATOR:
      return extensions::MenuItem::SEPARATOR;
  }
  return extensions::MenuItem::NORMAL;
}

template<typename PropertyWithEnumT>
scoped_ptr<extensions::MenuItem::Id> GetParentId(
    const PropertyWithEnumT& property,
    bool is_off_the_record,
    std::string extension_id) {
  if (!property.parent_id)
    return scoped_ptr<extensions::MenuItem::Id>();

  scoped_ptr<extensions::MenuItem::Id> parent_id(
      new extensions::MenuItem::Id(is_off_the_record, extension_id));
  if (property.parent_id->as_integer)
    parent_id->uid = *property.parent_id->as_integer;
  else if (property.parent_id->as_string)
    parent_id->string_uid = *property.parent_id->as_string;
  else
    NOTREACHED();
  return parent_id.Pass();
}

extensions::MenuItem* GetParent(extensions::MenuItem::Id parent_id,
                                const extensions::MenuManager* menu_manager,
                                std::string* error) {
  extensions::MenuItem* parent = menu_manager->GetItemById(parent_id);
  if (!parent) {
    *error = ErrorUtils::FormatErrorMessage(
        kCannotFindItemError, GetIDString(parent_id));
    return NULL;
  }
  if (parent->type() != extensions::MenuItem::NORMAL) {
    *error = kParentsMustBeNormalError;
    return NULL;
  }

  return parent;
}

}  // namespace

namespace extensions {

namespace Create = api::context_menus::Create;
namespace Remove = api::context_menus::Remove;
namespace Update = api::context_menus::Update;

bool ContextMenusCreateFunction::RunImpl() {
  MenuItem::Id id(profile()->IsOffTheRecord(), extension_id());
  scoped_ptr<Create::Params> params(Create::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (params->create_properties.id.get()) {
    id.string_uid = *params->create_properties.id;
  } else {
    if (GetExtension()->has_lazy_background_page()) {
      error_ = kIdRequiredError;
      return false;
    }

    // The Generated Id is added by context_menus_custom_bindings.js.
    DictionaryValue* properties = NULL;
    EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &properties));
    EXTENSION_FUNCTION_VALIDATE(properties->GetInteger(kGeneratedIdKey,
                                                       &id.uid));
  }

  std::string title;
  if (params->create_properties.title.get())
    title = *params->create_properties.title;

  MenuManager* menu_manager = profile()->GetExtensionService()->menu_manager();

  if (menu_manager->GetItemById(id)) {
    error_ = ErrorUtils::FormatErrorMessage(kDuplicateIDError,
                                                     GetIDString(id));
    return false;
  }

  if (GetExtension()->has_lazy_background_page() &&
      params->create_properties.onclick.get()) {
    error_ = kOnclickDisallowedError;
    return false;
  }

  MenuItem::ContextList contexts;
  if (params->create_properties.contexts.get())
    contexts = GetContexts(params->create_properties);
  else
    contexts.Add(MenuItem::PAGE);

  if (contexts.Contains(MenuItem::LAUNCHER) &&
      !GetExtension()->is_platform_app()) {
    error_ = kLauncherNotAllowedError;
    return false;
  }

  MenuItem::Type type = GetType(params->create_properties);

  if (title.empty() && type != MenuItem::SEPARATOR) {
    error_ = kTitleNeededError;
    return false;
  }

  bool checked = false;
  if (params->create_properties.checked.get())
    checked = *params->create_properties.checked;

  bool enabled = true;
  if (params->create_properties.enabled.get())
    enabled = *params->create_properties.enabled;

  scoped_ptr<MenuItem> item(
      new MenuItem(id, title, checked, enabled, type, contexts));

  if (!item->PopulateURLPatterns(
          params->create_properties.document_url_patterns.get(),
          params->create_properties.target_url_patterns.get(),
          &error_)) {
    return false;
  }

  bool success = true;
  scoped_ptr<MenuItem::Id> parent_id(GetParentId(params->create_properties,
                                                 profile()->IsOffTheRecord(),
                                                 extension_id()));
  if (parent_id.get()) {
    MenuItem* parent = GetParent(*parent_id, menu_manager, &error_);
    if (!parent)
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

bool ContextMenusUpdateFunction::RunImpl() {
  bool radio_item_updated = false;
  MenuItem::Id item_id(profile()->IsOffTheRecord(), extension_id());
  scoped_ptr<Update::Params> params(Update::Params::Create(*args_));

  EXTENSION_FUNCTION_VALIDATE(params.get());
  if (params->id.as_string)
    item_id.string_uid = *params->id.as_string;
  else if (params->id.as_integer)
    item_id.uid = *params->id.as_integer;
  else
    NOTREACHED();

  ExtensionService* service = profile()->GetExtensionService();
  MenuManager* manager = service->menu_manager();
  MenuItem* item = manager->GetItemById(item_id);
  if (!item || item->extension_id() != extension_id()) {
    error_ = ErrorUtils::FormatErrorMessage(
        kCannotFindItemError, GetIDString(item_id));
    return false;
  }

  // Type.
  MenuItem::Type type = GetType(params->update_properties);

  if (type != item->type()) {
    if (type == MenuItem::RADIO || item->type() == MenuItem::RADIO)
      radio_item_updated = true;
    item->set_type(type);
  }

  // Title.
  if (params->update_properties.title.get()) {
    std::string title(*params->update_properties.title);
    if (title.empty() && item->type() != MenuItem::SEPARATOR) {
      error_ = kTitleNeededError;
      return false;
    }
    item->set_title(title);
  }

  // Checked state.
  if (params->update_properties.checked.get()) {
    bool checked = *params->update_properties.checked;
    if (checked &&
        item->type() != MenuItem::CHECKBOX &&
        item->type() != MenuItem::RADIO) {
      error_ = kCheckedError;
      return false;
    }
    if (checked != item->checked()) {
      if (!item->SetChecked(checked)) {
        error_ = kCheckedError;
        return false;
      }
      radio_item_updated = true;
    }
  }

  // Enabled.
  if (params->update_properties.enabled.get())
    item->set_enabled(*params->update_properties.enabled);

  // Contexts.
  MenuItem::ContextList contexts;
  if (params->update_properties.contexts.get()) {
    contexts = GetContexts(params->update_properties);

    if (contexts.Contains(MenuItem::LAUNCHER) &&
        !GetExtension()->is_platform_app()) {
      error_ = kLauncherNotAllowedError;
      return false;
    }

    if (contexts != item->contexts())
      item->set_contexts(contexts);
  }

  // Parent id.
  MenuItem* parent = NULL;
  scoped_ptr<MenuItem::Id> parent_id(GetParentId(params->update_properties,
                                                 profile()->IsOffTheRecord(),
                                                 extension_id()));
  if (parent_id.get()) {
    MenuItem* parent = GetParent(*parent_id, manager, &error_);
    if (!parent || !manager->ChangeParent(item->id(), &parent->id()))
      return false;
  }

  // URL Patterns.
  if (!item->PopulateURLPatterns(
          params->update_properties.document_url_patterns.get(),
          params->update_properties.target_url_patterns.get(), &error_)) {
    return false;
  }

  // There is no need to call ItemUpdated if ChangeParent is called because
  // all sanitation is taken care of in ChangeParent.
  if (!parent && radio_item_updated && !manager->ItemUpdated(item->id()))
    return false;

  manager->WriteToStorage(GetExtension());
  return true;
}

bool ContextMenusRemoveFunction::RunImpl() {
  scoped_ptr<Remove::Params> params(Remove::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  ExtensionService* service = profile()->GetExtensionService();
  MenuManager* manager = service->menu_manager();

  MenuItem::Id id(profile()->IsOffTheRecord(), extension_id());
  if (params->menu_item_id.as_string)
    id.string_uid = *params->menu_item_id.as_string;
  else if (params->menu_item_id.as_integer)
    id.uid = *params->menu_item_id.as_integer;
  else
    NOTREACHED();

  MenuItem* item = manager->GetItemById(id);
  // Ensure one extension can't remove another's menu items.
  if (!item || item->extension_id() != extension_id()) {
    error_ = ErrorUtils::FormatErrorMessage(
        kCannotFindItemError, GetIDString(id));
    return false;
  }

  if (!manager->RemoveContextMenuItem(id))
    return false;
  manager->WriteToStorage(GetExtension());
  return true;
}

bool ContextMenusRemoveAllFunction::RunImpl() {
  ExtensionService* service = profile()->GetExtensionService();
  MenuManager* manager = service->menu_manager();
  manager->RemoveAllContextItems(GetExtension()->id());
  manager->WriteToStorage(GetExtension());
  return true;
}

}  // namespace extensions

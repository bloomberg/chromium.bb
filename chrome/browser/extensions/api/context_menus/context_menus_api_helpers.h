// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Definition of helper functions for the ContextMenus API.

#ifndef CHROME_BROWSER_EXTENSIONS_API_CONTEXT_MENUS_CONTEXT_MENUS_API_HELPERS_H_
#define CHROME_BROWSER_EXTENSIONS_API_CONTEXT_MENUS_CONTEXT_MENUS_API_HELPERS_H_

#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_handlers/background_info.h"

namespace extensions {
namespace context_menus_api_helpers {

namespace {

template <typename PropertyWithEnumT>
scoped_ptr<extensions::MenuItem::Id> GetParentId(
    const PropertyWithEnumT& property,
    bool is_off_the_record,
    const MenuItem::ExtensionKey& key) {
  if (!property.parent_id)
    return scoped_ptr<extensions::MenuItem::Id>();

  scoped_ptr<extensions::MenuItem::Id> parent_id(
      new extensions::MenuItem::Id(is_off_the_record, key));
  if (property.parent_id->as_integer)
    parent_id->uid = *property.parent_id->as_integer;
  else if (property.parent_id->as_string)
    parent_id->string_uid = *property.parent_id->as_string;
  else
    NOTREACHED();
  return parent_id.Pass();
}

}  // namespace

extern const char kCannotFindItemError[];
extern const char kCheckedError[];
extern const char kDuplicateIDError[];
extern const char kGeneratedIdKey[];
extern const char kLauncherNotAllowedError[];
extern const char kOnclickDisallowedError[];
extern const char kParentsMustBeNormalError[];
extern const char kTitleNeededError[];

std::string GetIDString(const MenuItem::Id& id);

MenuItem* GetParent(MenuItem::Id parent_id,
                    const MenuManager* menu_manager,
                    std::string* error);

template<typename PropertyWithEnumT>
MenuItem::ContextList GetContexts(const PropertyWithEnumT& property) {
  MenuItem::ContextList contexts;
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
        // Not available for <webview>.
        contexts.Add(extensions::MenuItem::LAUNCHER);
        break;
      case PropertyWithEnumT::CONTEXTS_TYPE_NONE:
        NOTREACHED();
    }
  }
  return contexts;
}

template<typename PropertyWithEnumT>
MenuItem::Type GetType(const PropertyWithEnumT& property,
                       MenuItem::Type default_type) {
  switch (property.type) {
    case PropertyWithEnumT::TYPE_NONE:
      return default_type;
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

// Creates and adds a menu item from |create_properties|.
template<typename PropertyWithEnumT>
bool CreateMenuItem(const PropertyWithEnumT& create_properties,
                    Profile* profile,
                    const Extension* extension,
                    const MenuItem::Id& item_id,
                    std::string* error) {
  bool is_webview = item_id.extension_key.webview_instance_id != 0;
  MenuManager* menu_manager = MenuManager::Get(profile);

  if (menu_manager->GetItemById(item_id)) {
    *error = ErrorUtils::FormatErrorMessage(kDuplicateIDError,
                                            GetIDString(item_id));
    return false;
  }

  if (!is_webview && BackgroundInfo::HasLazyBackgroundPage(extension) &&
      create_properties.onclick.get()) {
    *error = kOnclickDisallowedError;
    return false;
  }

  // Contexts.
  MenuItem::ContextList contexts;
  if (create_properties.contexts.get())
    contexts = GetContexts(create_properties);
  else
    contexts.Add(MenuItem::PAGE);

  if (contexts.Contains(MenuItem::LAUNCHER)) {
    // Launcher item is not allowed for <webview>.
    if (!extension->is_platform_app() || is_webview) {
      *error = kLauncherNotAllowedError;
      return false;
    }
  }

  // Title.
  std::string title;
  if (create_properties.title.get())
    title = *create_properties.title;

  MenuItem::Type type = GetType(create_properties, MenuItem::NORMAL);
  if (title.empty() && type != MenuItem::SEPARATOR) {
    *error = kTitleNeededError;
    return false;
  }

  // Checked state.
  bool checked = false;
  if (create_properties.checked.get())
    checked = *create_properties.checked;

  // Enabled.
  bool enabled = true;
  if (create_properties.enabled.get())
    enabled = *create_properties.enabled;

  scoped_ptr<MenuItem> item(
      new MenuItem(item_id, title, checked, enabled, type, contexts));

  // URL Patterns.
  if (!item->PopulateURLPatterns(
          create_properties.document_url_patterns.get(),
          create_properties.target_url_patterns.get(),
          error)) {
    return false;
  }

  // Parent id.
  bool success = true;
  scoped_ptr<MenuItem::Id> parent_id(GetParentId(
      create_properties, profile->IsOffTheRecord(), item_id.extension_key));
  if (parent_id.get()) {
    MenuItem* parent = GetParent(*parent_id, menu_manager, error);
    if (!parent)
      return false;
    success = menu_manager->AddChildItem(parent->id(), item.release());
  } else {
    success = menu_manager->AddContextItem(extension, item.release());
  }

  if (!success)
    return false;

  menu_manager->WriteToStorage(extension, item_id.extension_key);
  return true;
}

// Updates a menu item from |update_properties|.
template<typename PropertyWithEnumT>
bool UpdateMenuItem(const PropertyWithEnumT& update_properties,
                    Profile* profile,
                    const Extension* extension,
                    const MenuItem::Id& item_id,
                    std::string* error) {
  bool radio_item_updated = false;
  bool is_webview = item_id.extension_key.webview_instance_id != 0;
  MenuManager* menu_manager = MenuManager::Get(profile);

  MenuItem* item = menu_manager->GetItemById(item_id);
  if (!item || item->extension_id() != extension->id()){
    *error = ErrorUtils::FormatErrorMessage(
        kCannotFindItemError, GetIDString(item_id));
    return false;
  }

  // Type.
  MenuItem::Type type = GetType(update_properties, item->type());

  if (type != item->type()) {
    if (type == MenuItem::RADIO || item->type() == MenuItem::RADIO)
      radio_item_updated = true;
    item->set_type(type);
  }

  // Title.
  if (update_properties.title.get()) {
    std::string title(*update_properties.title);
    if (title.empty() && item->type() != MenuItem::SEPARATOR) {
      *error = kTitleNeededError;
      return false;
    }
    item->set_title(title);
  }

  // Checked state.
  if (update_properties.checked.get()) {
    bool checked = *update_properties.checked;
    if (checked &&
        item->type() != MenuItem::CHECKBOX &&
        item->type() != MenuItem::RADIO) {
      *error = kCheckedError;
      return false;
    }
    if (checked != item->checked()) {
      if (!item->SetChecked(checked)) {
        *error = kCheckedError;
        return false;
      }
      radio_item_updated = true;
    }
  }

  // Enabled.
  if (update_properties.enabled.get())
    item->set_enabled(*update_properties.enabled);

  // Contexts.
  MenuItem::ContextList contexts;
  if (update_properties.contexts.get()) {
    contexts = GetContexts(update_properties);

    if (contexts.Contains(MenuItem::LAUNCHER)) {
      // Launcher item is not allowed for <webview>.
      if (!extension->is_platform_app() || is_webview) {
        *error = kLauncherNotAllowedError;
        return false;
      }
    }

    if (contexts != item->contexts())
      item->set_contexts(contexts);
  }

  // Parent id.
  MenuItem* parent = NULL;
  scoped_ptr<MenuItem::Id> parent_id(GetParentId(
      update_properties, profile->IsOffTheRecord(), item_id.extension_key));
  if (parent_id.get()) {
    MenuItem* parent = GetParent(*parent_id, menu_manager, error);
    if (!parent || !menu_manager->ChangeParent(item->id(), &parent->id()))
      return false;
  }

  // URL Patterns.
  if (!item->PopulateURLPatterns(
          update_properties.document_url_patterns.get(),
          update_properties.target_url_patterns.get(), error)) {
    return false;
  }

  // There is no need to call ItemUpdated if ChangeParent is called because
  // all sanitation is taken care of in ChangeParent.
  if (!parent && radio_item_updated && !menu_manager->ItemUpdated(item->id()))
    return false;

  menu_manager->WriteToStorage(extension, item_id.extension_key);
  return true;
}

}  // namespace context_menus_api_helpers
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_CONTEXT_MENUS_CONTEXT_MENUS_API_HELPERS_H_

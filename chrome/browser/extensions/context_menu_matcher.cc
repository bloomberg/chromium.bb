// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/context_menu_matcher.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/context_menu_params.h"
#include "extensions/browser/extension_system.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/common/extensions/api/context_menus.h"
#endif

namespace extensions {

namespace {

int GetActionMenuTopLevelLimit() {
#if defined(ENABLE_EXTENSIONS)
  return api::context_menus::ACTION_MENU_TOP_LEVEL_LIMIT;
#else
  return 0;
#endif
}

// The range of command IDs reserved for extension's custom menus.
// TODO(oshima): These values will be injected by embedders.
int extensions_context_custom_first = IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST;
int extensions_context_custom_last = IDC_EXTENSIONS_CONTEXT_CUSTOM_LAST;

}  // namespace

// static
const size_t ContextMenuMatcher::kMaxExtensionItemTitleLength = 75;

// static
int ContextMenuMatcher::ConvertToExtensionsCustomCommandId(int id) {
  return extensions_context_custom_first + id;
}

// static
bool ContextMenuMatcher::IsExtensionsCustomCommandId(int id) {
  return id >= extensions_context_custom_first &&
         id <= extensions_context_custom_last;
}

ContextMenuMatcher::ContextMenuMatcher(
    content::BrowserContext* browser_context,
    ui::SimpleMenuModel::Delegate* delegate,
    ui::SimpleMenuModel* menu_model,
    const base::Callback<bool(const MenuItem*)>& filter)
    : browser_context_(browser_context),
      menu_model_(menu_model),
      delegate_(delegate),
      filter_(filter) {
}

void ContextMenuMatcher::AppendExtensionItems(
    const MenuItem::ExtensionKey& extension_key,
    const base::string16& selection_text,
    int* index,
    bool is_action_menu) {
  DCHECK_GE(*index, 0);
  int max_index =
      extensions_context_custom_last - extensions_context_custom_first;
  if (*index >= max_index)
    return;

  const Extension* extension = NULL;
  MenuItem::List items;
  bool can_cross_incognito;
  if (!GetRelevantExtensionTopLevelItems(
          extension_key, &extension, &can_cross_incognito, items))
    return;

  if (items.empty())
    return;

  // If this is the first extension-provided menu item, and there are other
  // items in the menu, and the last item is not a separator add a separator.
  if (*index == 0 && menu_model_->GetItemCount())
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);

  // Extensions (other than platform apps) are only allowed one top-level slot
  // (and it can't be a radio or checkbox item because we are going to put the
  // extension icon next to it), unless the context menu is an an action menu.
  // Action menus do not include the extension action, and they only include
  // items from one extension, so they are not placed within a submenu.
  // Otherwise, we automatically push them into a submenu if there is more than
  // one top-level item.
  if (extension->is_platform_app() || is_action_menu) {
    RecursivelyAppendExtensionItems(items,
                                    can_cross_incognito,
                                    selection_text,
                                    menu_model_,
                                    index,
                                    is_action_menu);
  } else {
    int menu_id = ConvertToExtensionsCustomCommandId(*index);
    (*index)++;
    base::string16 title;
    MenuItem::List submenu_items;

    if (items.size() > 1 || items[0]->type() != MenuItem::NORMAL) {
      title = base::UTF8ToUTF16(extension->name());
      submenu_items = items;
    } else {
      MenuItem* item = items[0];
      extension_item_map_[menu_id] = item->id();
      title = item->TitleWithReplacement(selection_text,
                                       kMaxExtensionItemTitleLength);
      submenu_items = GetRelevantExtensionItems(item->children(),
                                                can_cross_incognito);
    }

    // Now add our item(s) to the menu_model_.
    if (submenu_items.empty()) {
      menu_model_->AddItem(menu_id, title);
    } else {
      ui::SimpleMenuModel* submenu = new ui::SimpleMenuModel(delegate_);
      extension_menu_models_.push_back(submenu);
      menu_model_->AddSubMenu(menu_id, title, submenu);
      RecursivelyAppendExtensionItems(submenu_items,
                                      can_cross_incognito,
                                      selection_text,
                                      submenu,
                                      index,
                                      false);  // is_action_menu_top_level
    }
    if (!is_action_menu)
      SetExtensionIcon(extension_key.extension_id);
  }
}

void ContextMenuMatcher::Clear() {
  extension_item_map_.clear();
  extension_menu_models_.clear();
}

base::string16 ContextMenuMatcher::GetTopLevelContextMenuTitle(
    const MenuItem::ExtensionKey& extension_key,
    const base::string16& selection_text) {
  const Extension* extension = NULL;
  MenuItem::List items;
  bool can_cross_incognito;
  GetRelevantExtensionTopLevelItems(
      extension_key, &extension, &can_cross_incognito, items);

  base::string16 title;

  if (items.empty() ||
      items.size() > 1 ||
      items[0]->type() != MenuItem::NORMAL) {
    title = base::UTF8ToUTF16(extension->name());
  } else {
    MenuItem* item = items[0];
    title = item->TitleWithReplacement(
        selection_text, kMaxExtensionItemTitleLength);
  }
  return title;
}

bool ContextMenuMatcher::IsCommandIdChecked(int command_id) const {
  MenuItem* item = GetExtensionMenuItem(command_id);
  if (!item)
    return false;
  return item->checked();
}

bool ContextMenuMatcher::IsCommandIdEnabled(int command_id) const {
  MenuItem* item = GetExtensionMenuItem(command_id);
  if (!item)
    return true;
  return item->enabled();
}

void ContextMenuMatcher::ExecuteCommand(int command_id,
    content::WebContents* web_contents,
    const content::ContextMenuParams& params) {
  MenuItem* item = GetExtensionMenuItem(command_id);
  if (!item)
    return;

  MenuManager* manager = MenuManager::Get(browser_context_);
  manager->ExecuteCommand(browser_context_, web_contents, params, item->id());
}

bool ContextMenuMatcher::GetRelevantExtensionTopLevelItems(
    const MenuItem::ExtensionKey& extension_key,
    const Extension** extension,
    bool* can_cross_incognito,
    MenuItem::List& items) {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(browser_context_)->extension_service();
  *extension = service->GetExtensionById(extension_key.extension_id, false);

  if (!*extension)
    return false;

  // Find matching items.
  MenuManager* manager = MenuManager::Get(browser_context_);
  const MenuItem::List* all_items = manager->MenuItems(extension_key);
  if (!all_items || all_items->empty())
    return false;

  *can_cross_incognito = util::CanCrossIncognito(*extension, browser_context_);
  items = GetRelevantExtensionItems(*all_items,
                                    *can_cross_incognito);

  return true;
}

MenuItem::List ContextMenuMatcher::GetRelevantExtensionItems(
    const MenuItem::List& items,
    bool can_cross_incognito) {
  MenuItem::List result;
  for (MenuItem::List::const_iterator i = items.begin();
       i != items.end(); ++i) {
    const MenuItem* item = *i;

    if (!filter_.Run(item))
      continue;

    if (item->id().incognito == browser_context_->IsOffTheRecord() ||
        can_cross_incognito)
      result.push_back(*i);
  }
  return result;
}

void ContextMenuMatcher::RecursivelyAppendExtensionItems(
    const MenuItem::List& items,
    bool can_cross_incognito,
    const base::string16& selection_text,
    ui::SimpleMenuModel* menu_model,
    int* index,
    bool is_action_menu_top_level) {
  MenuItem::Type last_type = MenuItem::NORMAL;
  int radio_group_id = 1;
  int num_items = 0;

  for (MenuItem::List::const_iterator i = items.begin();
       i != items.end(); ++i) {
    MenuItem* item = *i;

    // If last item was of type radio but the current one isn't, auto-insert
    // a separator.  The converse case is handled below.
    if (last_type == MenuItem::RADIO &&
        item->type() != MenuItem::RADIO) {
      menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
      last_type = MenuItem::SEPARATOR;
    }

    int menu_id = ConvertToExtensionsCustomCommandId(*index);
    ++(*index);
    ++num_items;
    // Action context menus have a limit for top level extension items to
    // prevent control items from being pushed off the screen, since extension
    // items will not be placed in a submenu.
    if (menu_id >= extensions_context_custom_last ||
        (is_action_menu_top_level && num_items >= GetActionMenuTopLevelLimit()))
      return;

    extension_item_map_[menu_id] = item->id();
    base::string16 title = item->TitleWithReplacement(selection_text,
                                                kMaxExtensionItemTitleLength);
    if (item->type() == MenuItem::NORMAL) {
      MenuItem::List children =
          GetRelevantExtensionItems(item->children(), can_cross_incognito);
      if (children.empty()) {
        menu_model->AddItem(menu_id, title);
      } else {
        ui::SimpleMenuModel* submenu = new ui::SimpleMenuModel(delegate_);
        extension_menu_models_.push_back(submenu);
        menu_model->AddSubMenu(menu_id, title, submenu);
        RecursivelyAppendExtensionItems(children,
                                        can_cross_incognito,
                                        selection_text,
                                        submenu,
                                        index,
                                        false);  // is_action_menu_top_level
      }
    } else if (item->type() == MenuItem::CHECKBOX) {
      menu_model->AddCheckItem(menu_id, title);
    } else if (item->type() == MenuItem::RADIO) {
      if (i != items.begin() &&
          last_type != MenuItem::RADIO) {
        radio_group_id++;

        // Auto-append a separator if needed.
        menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
      }

      menu_model->AddRadioItem(menu_id, title, radio_group_id);
    } else if (item->type() == MenuItem::SEPARATOR) {
      menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
    }
    last_type = item->type();
  }
}

MenuItem* ContextMenuMatcher::GetExtensionMenuItem(int id) const {
  MenuManager* manager = MenuManager::Get(browser_context_);
  std::map<int, MenuItem::Id>::const_iterator i =
      extension_item_map_.find(id);
  if (i != extension_item_map_.end()) {
    MenuItem* item = manager->GetItemById(i->second);
    if (item)
      return item;
  }
  return NULL;
}

void ContextMenuMatcher::SetExtensionIcon(const std::string& extension_id) {
  MenuManager* menu_manager = MenuManager::Get(browser_context_);

  int index = menu_model_->GetItemCount() - 1;
  DCHECK_GE(index, 0);

  const SkBitmap& icon = menu_manager->GetIconForExtension(extension_id);
  DCHECK(icon.width() == gfx::kFaviconSize);
  DCHECK(icon.height() == gfx::kFaviconSize);

  menu_model_->SetIcon(index, gfx::Image::CreateFrom1xBitmap(icon));
}

}  // namespace extensions

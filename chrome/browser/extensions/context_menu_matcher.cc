// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/common/context_menu_params.h"
#include "ui/gfx/favicon_size.h"

namespace extensions {

// static
const size_t ContextMenuMatcher::kMaxExtensionItemTitleLength = 75;

ContextMenuMatcher::ContextMenuMatcher(
    Profile* profile,
    ui::SimpleMenuModel::Delegate* delegate,
    ui::SimpleMenuModel* menu_model,
    const base::Callback<bool(const MenuItem*)>& filter)
    : profile_(profile), menu_model_(menu_model), delegate_(delegate),
      filter_(filter) {
}

void ContextMenuMatcher::AppendExtensionItems(const std::string& extension_id,
                                              const string16& selection_text,
                                              int* index)
{
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  MenuManager* manager = service->menu_manager();
  const Extension* extension = service->GetExtensionById(extension_id, false);
  DCHECK_GE(*index, 0);
  int max_index =
      IDC_EXTENSIONS_CONTEXT_CUSTOM_LAST - IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST;
  if (!extension || *index >= max_index)
    return;

  // Find matching items.
  const MenuItem::List* all_items = manager->MenuItems(extension_id);
  if (!all_items || all_items->empty())
    return;
  bool can_cross_incognito = service->CanCrossIncognito(extension);
  MenuItem::List items = GetRelevantExtensionItems(*all_items,
                                                   can_cross_incognito);

  if (items.empty())
    return;

  // If this is the first extension-provided menu item, and there are other
  // items in the menu, and the last item is not a separator add a separator.
  if (*index == 0 && menu_model_->GetItemCount() &&
      menu_model_->GetTypeAt(menu_model_->GetItemCount() - 1) !=
          ui::MenuModel::TYPE_SEPARATOR)
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);

  // Extensions (other than platform apps) are only allowed one top-level slot
  // (and it can't be a radio or checkbox item because we are going to put the
  // extension icon next to it).
  // If they have more than that, we automatically push them into a submenu.
  if (extension->is_platform_app()) {
    RecursivelyAppendExtensionItems(items, can_cross_incognito, selection_text,
                                    menu_model_, index);
  } else {
    int menu_id = IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST + (*index)++;
    string16 title;
    MenuItem::List submenu_items;

    if (items.size() > 1 || items[0]->type() != MenuItem::NORMAL) {
      title = UTF8ToUTF16(extension->name());
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
      RecursivelyAppendExtensionItems(submenu_items, can_cross_incognito,
                                      selection_text, submenu, index);
    }
    SetExtensionIcon(extension_id);
  }
}

void ContextMenuMatcher::Clear() {
  extension_item_map_.clear();
  extension_menu_models_.clear();
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
  MenuManager* manager = extensions::ExtensionSystem::Get(profile_)->
      extension_service()->menu_manager();
  MenuItem* item = GetExtensionMenuItem(command_id);
  if (!item)
    return;

  manager->ExecuteCommand(profile_, web_contents, params, item->id());
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

    if (item->id().incognito == profile_->IsOffTheRecord() ||
        can_cross_incognito)
      result.push_back(*i);
  }
  return result;
}

void ContextMenuMatcher::RecursivelyAppendExtensionItems(
    const MenuItem::List& items,
    bool can_cross_incognito,
    const string16& selection_text,
    ui::SimpleMenuModel* menu_model,
    int* index)
{
  MenuItem::Type last_type = MenuItem::NORMAL;
  int radio_group_id = 1;

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

    int menu_id = IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST + (*index)++;
    if (menu_id >= IDC_EXTENSIONS_CONTEXT_CUSTOM_LAST)
      return;
    extension_item_map_[menu_id] = item->id();
    string16 title = item->TitleWithReplacement(selection_text,
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
        RecursivelyAppendExtensionItems(children, can_cross_incognito,
                                        selection_text, submenu, index);
      }
    } else if (item->type() == MenuItem::CHECKBOX) {
      menu_model->AddCheckItem(menu_id, title);
    } else if (item->type() == MenuItem::RADIO) {
      if (i != items.begin() &&
          last_type != MenuItem::RADIO) {
        radio_group_id++;

        // Auto-append a separator if needed.
        if (last_type != MenuItem::SEPARATOR)
          menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
      }

      menu_model->AddRadioItem(menu_id, title, radio_group_id);
    } else if (item->type() == MenuItem::SEPARATOR) {
      if (i != items.begin() && last_type != MenuItem::SEPARATOR) {
        menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
      }
    }
    last_type = item->type();
  }
}

MenuItem* ContextMenuMatcher::GetExtensionMenuItem(int id) const {
  MenuManager* manager = extensions::ExtensionSystem::Get(profile_)->
      extension_service()->menu_manager();
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
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  MenuManager* menu_manager = service->menu_manager();

  int index = menu_model_->GetItemCount() - 1;
  DCHECK_GE(index, 0);

  const SkBitmap& icon = menu_manager->GetIconForExtension(extension_id);
  DCHECK(icon.width() == gfx::kFaviconSize);
  DCHECK(icon.height() == gfx::kFaviconSize);

  menu_model_->SetIcon(index, gfx::Image::CreateFrom1xBitmap(icon));
}

}  // namespace extensions

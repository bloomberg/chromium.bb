// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/action_box_menu_model.h"

#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_to_mobile_service.h"
#include "chrome/browser/chrome_to_mobile_service_factory.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_toolbar_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// Extensions get command IDs that are beyond the maximal valid extension ID
// (0xDFFF) so that they are not confused with actual commands that appear in
// the menu. For more details see: chrome/app/chrome_command_ids.h
//
const int kFirstExtensionCommandId = 0xE000;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ActionBoxMenuModel

ActionBoxMenuModel::ActionBoxMenuModel(Browser* browser)
    : ALLOW_THIS_IN_INITIALIZER_LIST(ui::SimpleMenuModel(this)),
      browser_(browser) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  if (!browser_->profile()->IsOffTheRecord() &&
      ChromeToMobileServiceFactory::GetForProfile(browser_->profile())->
      HasMobiles()) {
    AddItemWithStringId(IDC_CHROME_TO_MOBILE_PAGE,
                        IDS_CHROME_TO_MOBILE_BUBBLE_TOOLTIP);
    SetIcon(GetIndexOfCommandId(IDC_CHROME_TO_MOBILE_PAGE),
            rb.GetNativeImageNamed(IDR_MOBILE));
  }

  TabContents* current_tab_contents = chrome::GetActiveTabContents(browser_);
  bool starred = current_tab_contents->bookmark_tab_helper()->is_starred();
  AddItemWithStringId(IDC_BOOKMARK_PAGE,
                      starred ? IDS_TOOLTIP_STARRED : IDS_TOOLTIP_STAR);
  SetIcon(GetIndexOfCommandId(IDC_BOOKMARK_PAGE),
          rb.GetNativeImageNamed(starred ? IDR_STAR_LIT : IDR_STAR));

  AddItemWithStringId(IDC_SHARE_PAGE, IDS_SHARE_PAGE);
  SetIcon(GetIndexOfCommandId(IDC_SHARE_PAGE),
          rb.GetNativeImageNamed(IDR_SHARE));

  // Adds extensions to the model.
  int command_id = kFirstExtensionCommandId;
  const extensions::ExtensionList& action_box_items = GetActionBoxMenuItems();
  if (!action_box_items.empty()) {
    AddSeparator(ui::NORMAL_SEPARATOR);
    for (size_t i = 0; i < action_box_items.size(); ++i) {
      const extensions::Extension* extension = action_box_items[i];
      AddItem(command_id, UTF8ToUTF16(extension->name()));
      id_to_extension_id_map_[command_id++] = extension->id();
    }
  }
}

ActionBoxMenuModel::~ActionBoxMenuModel() {
  // Ensures parent destructor does not use a partially destroyed delegate.
  set_delegate(NULL);
}

bool ActionBoxMenuModel::IsItemExtension(int index) {
  return GetCommandIdAt(index) >= kFirstExtensionCommandId;
}

const extensions::Extension* ActionBoxMenuModel::GetExtensionAt(int index) {
  if (!IsItemExtension(index))
    return NULL;

  // ExtensionList is mutable, so need to get up-to-date extension.
  int command_id = GetCommandIdAt(index);
  IdToEntensionIdMap::const_iterator it =
      id_to_extension_id_map_.find(command_id);
  if (it == id_to_extension_id_map_.end())
    return NULL;
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(browser_->profile())->
          extension_service();
  return extension_service->GetExtensionById(it->second, false);
}

bool ActionBoxMenuModel::IsCommandIdChecked(int command_id) const {
  return false;
}

bool ActionBoxMenuModel::IsCommandIdEnabled(int command_id) const {
  return true;
}

bool ActionBoxMenuModel::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  return false;
}

void ActionBoxMenuModel::ExecuteCommand(int command_id) {
  if (command_id < kFirstExtensionCommandId)
    chrome::ExecuteCommand(browser_, command_id);
}

const extensions::ExtensionList& ActionBoxMenuModel::GetActionBoxMenuItems() {
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(browser_->profile())->
          extension_service();
  return extension_service->toolbar_model()->action_box_menu_items();
}

void ActionBoxMenuModel::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
}

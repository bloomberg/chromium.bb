// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/action_box_menu_model.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_to_mobile_service.h"
#include "chrome/browser/chrome_to_mobile_service_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_toolbar_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"

using extensions::ActionInfo;

////////////////////////////////////////////////////////////////////////////////
// ActionBoxMenuModel

ActionBoxMenuModel::ActionBoxMenuModel(Browser* browser,
                                       ui::SimpleMenuModel::Delegate* delegate)
    : ui::SimpleMenuModel(delegate),
      browser_(browser) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  // TODO(msw): Show the item as disabled for chrome: and file: scheme pages?
  if (ChromeToMobileService::UpdateAndGetCommandState(browser_)) {
    AddItemWithStringId(IDC_CHROME_TO_MOBILE_PAGE,
                        IDS_CHROME_TO_MOBILE_BUBBLE_TOOLTIP);
    SetIcon(GetIndexOfCommandId(IDC_CHROME_TO_MOBILE_PAGE),
            rb.GetNativeImageNamed(IDR_MOBILE));
  }

  // In some unit tests, GetActiveWebContents can return NULL.
  bool starred = browser_->tab_strip_model()->GetActiveWebContents() &&
      BookmarkTabHelper::FromWebContents(browser_->tab_strip_model()->
          GetActiveWebContents())->is_starred();

  AddItemWithStringId(IDC_BOOKMARK_PAGE_FROM_STAR,
                      starred ? IDS_TOOLTIP_STARRED : IDS_TOOLTIP_STAR);
  SetIcon(GetIndexOfCommandId(IDC_BOOKMARK_PAGE_FROM_STAR),
          rb.GetNativeImageNamed(starred ? IDR_STAR_LIT : IDR_STAR));
}

ActionBoxMenuModel::~ActionBoxMenuModel() {
}

void ActionBoxMenuModel::AddExtension(const extensions::Extension& extension,
                                      int command_id) {
  if (extension_ids_.empty())
    AddSeparator(ui::NORMAL_SEPARATOR);
  extension_ids_.push_back(extension.id());
  const ActionInfo* page_launcher_info =
      ActionInfo::GetPageLauncherInfo(&extension);
  DCHECK(page_launcher_info);
  AddItem(command_id, UTF8ToUTF16(page_launcher_info->default_title));
}

bool ActionBoxMenuModel::IsItemExtension(int index) {
  // The extensions are always at the end of the model.
  CHECK(index < GetItemCount());
  return index >= GetFirstExtensionIndex();
}

const extensions::Extension* ActionBoxMenuModel::GetExtensionAt(int index) {
  if (!IsItemExtension(index))
    return NULL;

  int index_in_extension_ids = index - GetFirstExtensionIndex();
  CHECK_GE(index_in_extension_ids, 0);
  CHECK_LT(index_in_extension_ids, static_cast<int>(extension_ids_.size()));

  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(browser_->profile())->
          extension_service();
  return extension_service->extensions()->GetByID(
      extension_ids_[index_in_extension_ids]);
}

void ActionBoxMenuModel::ExecuteCommand(int command_id) {
  delegate()->ExecuteCommand(command_id);
}

int ActionBoxMenuModel::GetFirstExtensionIndex() {
  return GetItemCount() - extension_ids_.size();
}

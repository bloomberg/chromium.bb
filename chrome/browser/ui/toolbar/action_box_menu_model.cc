// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/action_box_menu_model.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_toolbar_model.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"


using extensions::ActionInfo;

////////////////////////////////////////////////////////////////////////////////
// ActionBoxMenuModel

ActionBoxMenuModel::ActionBoxMenuModel(Profile* profile,
                                       ui::SimpleMenuModel::Delegate* delegate)
    : ui::SimpleMenuModel(delegate),
      profile_(profile) {

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
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  return extension_service->extensions()->GetByID(
      extension_ids_[index_in_extension_ids]);
}

void ActionBoxMenuModel::ExecuteCommand(int command_id) {
  delegate()->ExecuteCommand(command_id, 0);
}

int ActionBoxMenuModel::GetFirstExtensionIndex() {
  return GetItemCount() - extension_ids_.size();
}

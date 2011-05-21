// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/profile_menu_model.h"

#include "chrome/browser/profiles/profile_manager.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

ProfileMenuModel::ProfileMenuModel()
    : ALLOW_THIS_IN_INITIALIZER_LIST(ui::SimpleMenuModel(this)) {
  AddItem(COMMAND_CREATE_NEW_PROFILE, l10n_util::GetStringUTF16(
      IDS_PROFILES_CREATE_NEW_PROFILE_OPTION));
}

ProfileMenuModel::~ProfileMenuModel() {
}

// ui::SimpleMenuModel::Delegate implementation
bool ProfileMenuModel::IsCommandIdChecked(int command_id) const {
  return false;
}

bool ProfileMenuModel::IsCommandIdEnabled(int command_id) const {
  return true;
}

bool ProfileMenuModel::GetAcceleratorForCommandId(int command_id,
    ui::Accelerator* accelerator) {
  return false;
}

void ProfileMenuModel::ExecuteCommand(int command_id) {
  switch (command_id) {
    case COMMAND_CREATE_NEW_PROFILE:
      ProfileManager::CreateMultiProfileAsync();
      break;
    default:
      NOTREACHED();
      break;
  }
}


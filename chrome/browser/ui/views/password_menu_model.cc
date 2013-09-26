// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/password_menu_model.h"

#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/views/content_setting_bubble_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

PasswordMenuModel::PasswordMenuModel(
    ContentSettingBubbleModel* bubble_model,
    ContentSettingBubbleContents* bubble_contents)
    : ui::SimpleMenuModel(this),
      media_bubble_model_(bubble_model),
      media_bubble_contents_(bubble_contents) {
  AddItem(COMMAND_ID_NOPE,
          l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_CANCEL_DROP_DOWN));
  AddItem(COMMAND_ID_NEVER_FOR_THIS_SITE,
          l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_BLACKLIST_DROP_DOWN));
}

PasswordMenuModel::~PasswordMenuModel() {}

bool PasswordMenuModel::IsCommandIdChecked(int command_id) const {
  return false;
}

bool PasswordMenuModel::IsCommandIdEnabled(int command_id) const {
  return true;
}

bool PasswordMenuModel::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  return false;
}

void PasswordMenuModel::ExecuteCommand(int command_id,
                                       int event_flags) {
  switch (static_cast<CommandID>(command_id)) {
    case COMMAND_ID_NOPE:
      media_bubble_model_->OnDoneClicked();
      media_bubble_contents_->StartFade(false);
      break;
    case COMMAND_ID_NEVER_FOR_THIS_SITE: {
      media_bubble_model_->OnCancelClicked();
      media_bubble_contents_->StartFade(false);
      break;
    }
  }
}

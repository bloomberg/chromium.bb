// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/system_menu_model_delegate.h"

#include "base/prefs/pref_service.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

SystemMenuModelDelegate::SystemMenuModelDelegate(
    ui::AcceleratorProvider* provider,
    Browser* browser)
    : provider_(provider),
      browser_(browser) {
}

SystemMenuModelDelegate::~SystemMenuModelDelegate() {
}

bool SystemMenuModelDelegate::IsCommandIdChecked(int command_id) const {
  switch (command_id) {
    case IDC_USE_SYSTEM_TITLE_BAR: {
      PrefService* prefs = browser_->profile()->GetPrefs();
      return !prefs->GetBoolean(prefs::kUseCustomChromeFrame);
    }
    default:
      return false;
  }
}

bool SystemMenuModelDelegate::IsCommandIdEnabled(int command_id) const {
  return chrome::IsCommandEnabled(browser_, command_id);
}

bool SystemMenuModelDelegate::GetAcceleratorForCommandId(int command_id,
                                             ui::Accelerator* accelerator) {
  return provider_->GetAcceleratorForCommandId(command_id, accelerator);
}

bool SystemMenuModelDelegate::IsItemForCommandIdDynamic(int command_id) const {
  return command_id == IDC_RESTORE_TAB;
}

base::string16 SystemMenuModelDelegate::GetLabelForCommandId(
    int command_id) const {
  DCHECK_EQ(command_id, IDC_RESTORE_TAB);

  int string_id = IDS_RESTORE_TAB;
  if (IsCommandIdEnabled(command_id)) {
    TabRestoreService* trs =
        TabRestoreServiceFactory::GetForProfile(browser_->profile());
    if (trs && trs->entries().front()->type == TabRestoreService::WINDOW)
      string_id = IDS_RESTORE_WINDOW;
  }
  return l10n_util::GetStringUTF16(string_id);
}

void SystemMenuModelDelegate::ExecuteCommand(int command_id, int event_flags) {
  chrome::ExecuteCommand(browser_, command_id);
}

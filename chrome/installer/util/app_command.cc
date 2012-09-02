// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/app_command.h"

#include "base/logging.h"
#include "base/win/registry.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/work_item_list.h"

namespace installer {

namespace {

// Adds a work item to set |value_name| to DWORD 1 if |value_data| is true;
// adds a work item to remove |value_name| otherwise.
WorkItem* AddSetOptionalBoolRegValueWorkItem(
    HKEY predefined_root,
    const string16& key_path,
    const string16& value_name,
    bool value_data,
    WorkItemList* item_list) {
  if (value_data) {
    return item_list->AddSetRegValueWorkItem(predefined_root,
                                             key_path,
                                             value_name,
                                             static_cast<DWORD>(1),
                                             true);
  } else {
    return item_list->AddDeleteRegValueWorkItem(predefined_root,
                                                key_path,
                                                value_name);
  }
}

}  // namespace

AppCommand::AppCommand()
    : sends_pings_(false),
      is_web_accessible_(false),
      is_auto_run_on_os_upgrade_(false) {
}

AppCommand::AppCommand(const string16& command_line)
    : command_line_(command_line),
      sends_pings_(false),
      is_web_accessible_(false),
      is_auto_run_on_os_upgrade_(false) {
}

bool AppCommand::Initialize(const base::win::RegKey& key) {
  if (!key.Valid()) {
    LOG(DFATAL) << "Cannot initialize an AppCommand from an invalid key.";
    return false;
  }

  LONG result = ERROR_SUCCESS;
  string16 cmd_line;
  DWORD sends_pings = 0;
  DWORD is_web_acc = 0;
  DWORD is_auto_run_on_os_upgrade = 0;

  result = key.ReadValue(google_update::kRegCommandLineField, &cmd_line);
  if (result != ERROR_SUCCESS) {
    LOG(WARNING) << "Error reading " << google_update::kRegCommandLineField
                 << " value from registry: " << result;
    return false;
  }

  // Note: ReadValueDW only modifies its out param on success.
  key.ReadValueDW(google_update::kRegSendsPingsField, &sends_pings);
  key.ReadValueDW(google_update::kRegWebAccessibleField, &is_web_acc);
  key.ReadValueDW(google_update::kRegAutoRunOnOSUpgradeField,
                  &is_auto_run_on_os_upgrade);

  command_line_.swap(cmd_line);
  sends_pings_ = (sends_pings != 0);
  is_web_accessible_ = (is_web_acc != 0);
  is_auto_run_on_os_upgrade_ = (is_auto_run_on_os_upgrade != 0);

  return true;
}

void AppCommand::AddWorkItems(HKEY predefined_root,
                              const string16& command_path,
                              WorkItemList* item_list) const {
  item_list->AddCreateRegKeyWorkItem(predefined_root, command_path)
      ->set_log_message("creating AppCommand registry key");
  item_list->AddSetRegValueWorkItem(predefined_root, command_path,
                                    google_update::kRegCommandLineField,
                                    command_line_, true)
      ->set_log_message("setting AppCommand CommandLine registry value");
  AddSetOptionalBoolRegValueWorkItem(predefined_root,
                                     command_path,
                                     google_update::kRegSendsPingsField,
                                     sends_pings_,
                                     item_list)
      ->set_log_message("setting AppCommand SendsPings registry value");
  AddSetOptionalBoolRegValueWorkItem(predefined_root,
                                     command_path,
                                     google_update::kRegWebAccessibleField,
                                     is_web_accessible_,
                                     item_list)
      ->set_log_message("setting AppCommand WebAccessible registry value");
  AddSetOptionalBoolRegValueWorkItem(predefined_root,
                                     command_path,
                                     google_update::kRegAutoRunOnOSUpgradeField,
                                     is_auto_run_on_os_upgrade_,
                                     item_list)
      ->set_log_message("setting AppCommand AutoRunOnOSUpgrade registry value");
}

}  // namespace installer

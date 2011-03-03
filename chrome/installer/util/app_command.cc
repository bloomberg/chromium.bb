// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/app_command.h"

#include "base/logging.h"
#include "base/win/registry.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/work_item_list.h"

namespace installer {

AppCommand::AppCommand()
    : sends_pings_(false),
      is_web_accessible_(false) {
}

AppCommand::AppCommand(const std::wstring& command_line,
                       bool sends_pings,
                       bool is_web_accessible)
    : command_line_(command_line),
      sends_pings_(sends_pings),
      is_web_accessible_(is_web_accessible) {
}

bool AppCommand::Initialize(const base::win::RegKey& key) {
  if (!key.Valid()) {
    LOG(DFATAL) << "Cannot initialize an AppCommand from an invalid key.";
    return false;
  }

  LONG result = ERROR_SUCCESS;
  std::wstring cmd_line;
  DWORD sends_pings = 0;
  DWORD is_web_acc = 0;

  result = key.ReadValue(google_update::kRegCommandLineField, &cmd_line);
  if (result != ERROR_SUCCESS) {
    LOG(WARNING) << "Error reading " << google_update::kRegCommandLineField
                 << " value from registry: " << result;
    return false;
  }

  result = key.ReadValueDW(google_update::kRegSendsPingsField, &sends_pings);
  if (result != ERROR_SUCCESS) {
    LOG(WARNING) << "Error reading " << google_update::kRegSendsPingsField
                 << " value from registry: " << result;
    return false;
  }

  result = key.ReadValueDW(google_update::kRegWebAccessibleField, &is_web_acc);
  if (result != ERROR_SUCCESS) {
    LOG(WARNING) << "Error reading " << google_update::kRegWebAccessibleField
                 << " value from registry: " << result;
    return false;
  }

  command_line_.swap(cmd_line);
  sends_pings_ = (sends_pings != 0);
  is_web_accessible_ = (is_web_acc != 0);

  return true;
}

void AppCommand::AddWorkItems(HKEY predefined_root,
                              const std::wstring& command_path,
                              WorkItemList* item_list) const {
  const DWORD sends_pings = sends_pings_ ? 1U : 0U;
  const DWORD is_web_accessible = is_web_accessible_ ? 1U : 0U;

  item_list->AddCreateRegKeyWorkItem(predefined_root, command_path)
      ->set_log_message("creating quick-enable-cf command registry key");
  item_list->AddSetRegValueWorkItem(predefined_root, command_path,
                                    google_update::kRegCommandLineField,
                                    command_line_, true)
      ->set_log_message("setting quick-enable-cf CommandLine registry value");
  item_list->AddSetRegValueWorkItem(predefined_root, command_path,
                                    google_update::kRegSendsPingsField,
                                    sends_pings, true)
      ->set_log_message("setting quick-enable-cf SendsPings registry value");
  item_list->AddSetRegValueWorkItem(predefined_root, command_path,
                                    google_update::kRegWebAccessibleField,
                                    is_web_accessible, true)
      ->set_log_message("setting quick-enable-cf WebAccessible registry value");
}

}  // namespace installer

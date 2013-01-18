// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/commands/commands.h"

#include "chrome/browser/extensions/api/commands/command_service.h"

namespace {

base::DictionaryValue* CreateCommandValue(
    const extensions::Command& command, bool active) {
  DictionaryValue* result = new DictionaryValue();
  result->SetString("name", command.command_name());
  result->SetString("description", command.description());
  result->SetString("shortcut",
                    active ? command.accelerator().GetShortcutText() :
                             string16());
  return result;
}

}  // namespace

bool GetAllCommandsFunction::RunImpl() {
  ListValue* command_list = new ListValue();

  extensions::CommandService* command_service =
      extensions::CommandService::Get(profile_);

  extensions::Command browser_action;
  bool active = false;
  if (command_service->GetBrowserActionCommand(extension_->id(),
          extensions::CommandService::ALL,
          &browser_action,
          &active)) {
    command_list->Append(CreateCommandValue(browser_action, active));
  }

  extensions::Command page_action;
  if (command_service->GetPageActionCommand(extension_->id(),
          extensions::CommandService::ALL,
          &page_action,
          &active)) {
    command_list->Append(CreateCommandValue(page_action, active));
  }

  extensions::Command script_badge;
  if (command_service->GetScriptBadgeCommand(extension_->id(),
          extensions::CommandService::ALL,
          &script_badge,
          &active)) {
    command_list->Append(CreateCommandValue(script_badge, active));
  }

  extensions::CommandMap named_commands;
  command_service->GetNamedCommands(extension_->id(),
                                    extensions::CommandService::ALL,
                                    &named_commands);

  for (extensions::CommandMap::const_iterator iter = named_commands.begin();
       iter != named_commands.end(); ++iter) {
    ui::Accelerator shortcut_assigned =
        command_service->FindShortcutForCommand(
            extension_->id(), iter->second.command_name());
    active = (shortcut_assigned.key_code() != ui::VKEY_UNKNOWN);

    command_list->Append(CreateCommandValue(iter->second, active));
  }

  SetResult(command_list);
  return true;
}

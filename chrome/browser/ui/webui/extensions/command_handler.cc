// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/command_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/extension_keybinding_registry.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/extension_set.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

CommandHandler::CommandHandler(Profile* profile) : profile_(profile) {
}

CommandHandler::~CommandHandler() {
}

void CommandHandler::GetLocalizedValues(DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  localized_strings->SetString("extensionCommandsOverlay",
      l10n_util::GetStringUTF16(IDS_EXTENSION_COMMANDS_DIALOG_TITLE));
  localized_strings->SetString("extensionCommandsEmpty",
      l10n_util::GetStringUTF16(IDS_EXTENSION_COMMANDS_EMPTY));
  localized_strings->SetString("extensionCommandsInactive",
      l10n_util::GetStringUTF16(IDS_EXTENSION_COMMANDS_INACTIVE));
  localized_strings->SetString("extensionCommandsStartTyping",
      l10n_util::GetStringUTF16(IDS_EXTENSION_TYPE_SHORTCUT));
  localized_strings->SetString("extensionCommandsDelete",
      l10n_util::GetStringUTF16(IDS_EXTENSION_DELETE_SHORTCUT));
  localized_strings->SetString("ok", l10n_util::GetStringUTF16(IDS_OK));
}

void CommandHandler::RegisterMessages() {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile_));

  web_ui()->RegisterMessageCallback("extensionCommandsRequestExtensionsData",
      base::Bind(&CommandHandler::HandleRequestExtensionsData,
      base::Unretained(this)));
  web_ui()->RegisterMessageCallback("setShortcutHandlingSuspended",
      base::Bind(&CommandHandler::HandleSetShortcutHandlingSuspended,
      base::Unretained(this)));
  web_ui()->RegisterMessageCallback("setExtensionCommandShortcut",
      base::Bind(&CommandHandler::HandleSetExtensionCommandShortcut,
      base::Unretained(this)));
}

void CommandHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_EXTENSION_LOADED ||
         type == chrome::NOTIFICATION_EXTENSION_UNLOADED);
  UpdateCommandDataOnPage();
}

void CommandHandler::UpdateCommandDataOnPage() {
  DictionaryValue results;
  GetAllCommands(&results);
  web_ui()->CallJavascriptFunction(
      "ExtensionCommandsOverlay.returnExtensionsData", results);
}

void CommandHandler::HandleRequestExtensionsData(const ListValue* args) {
  UpdateCommandDataOnPage();
}

void CommandHandler::HandleSetExtensionCommandShortcut(
    const base::ListValue* args) {
  std::string extension_id;
  std::string command_name;
  std::string keystroke;
  if (!args->GetString(0, &extension_id) ||
      !args->GetString(1, &command_name) ||
      !args->GetString(2, &keystroke)) {
    NOTREACHED();
    return;
  }

  Profile* profile = Profile::FromWebUI(web_ui());
  CommandService* command_service = CommandService::Get(profile);
  command_service->UpdateKeybindingPrefs(extension_id, command_name, keystroke);

  UpdateCommandDataOnPage();
}

void CommandHandler::HandleSetShortcutHandlingSuspended(const ListValue* args) {
  bool suspended;
  if (args->GetBoolean(0, &suspended))
    ExtensionKeybindingRegistry::SetShortcutHandlingSuspended(suspended);
}

void CommandHandler::GetAllCommands(base::DictionaryValue* commands) {
  ListValue* results = new ListValue;

  Profile* profile = Profile::FromWebUI(web_ui());
  CommandService* command_service = CommandService::Get(profile);

  const ExtensionSet* extensions = extensions::ExtensionSystem::Get(profile)->
      extension_service()->extensions();
  for (ExtensionSet::const_iterator extension = extensions->begin();
       extension != extensions->end(); ++extension) {
    scoped_ptr<DictionaryValue> extension_dict(new DictionaryValue);
    extension_dict->SetString("name", (*extension)->name());
    extension_dict->SetString("id", (*extension)->id());

    // Add the keybindings to a list structure.
    scoped_ptr<ListValue> extensions_list(new ListValue());

    bool active = false;

    extensions::Command browser_action;
    if (command_service->GetBrowserActionCommand((*extension)->id(),
                                                 CommandService::ALL,
                                                 &browser_action,
                                                 &active)) {
      extensions_list->Append(browser_action.ToValue((*extension), active));
    }

    extensions::Command page_action;
    if (command_service->GetPageActionCommand((*extension)->id(),
                                              CommandService::ALL,
                                              &page_action,
                                              &active)) {
      extensions_list->Append(page_action.ToValue((*extension), active));
    }

    extensions::Command script_badge;
    if (command_service->GetScriptBadgeCommand((*extension)->id(),
                                              CommandService::ALL,
                                              &script_badge,
                                              &active)) {
      extensions_list->Append(script_badge.ToValue((*extension), active));
    }

    extensions::CommandMap named_commands;
    if (command_service->GetNamedCommands((*extension)->id(),
                                          CommandService::ALL,
                                          &named_commands)) {
      for (extensions::CommandMap::const_iterator iter = named_commands.begin();
           iter != named_commands.end(); ++iter) {
        ui::Accelerator shortcut_assigned =
            command_service->FindShortcutForCommand(
                (*extension)->id(), iter->second.command_name());
        active = (shortcut_assigned.key_code() != ui::VKEY_UNKNOWN);

        extensions_list->Append(iter->second.ToValue((*extension), active));
      }
    }

    if (!extensions_list->empty()) {
      extension_dict->Set("commands", extensions_list.release());
      results->Append(extension_dict.release());
    }
  }

  commands->Set("commands", results);
}

}  // namespace extensions

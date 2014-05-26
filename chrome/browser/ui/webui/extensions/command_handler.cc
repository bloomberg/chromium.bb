// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/command_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/extension_commands_global_registry.h"
#include "chrome/browser/extensions/extension_keybinding_registry.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_set.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

CommandHandler::CommandHandler(Profile* profile)
    : profile_(profile),
      extension_registry_observer_(this) {
}

CommandHandler::~CommandHandler() {
}

void CommandHandler::GetLocalizedValues(content::WebUIDataSource* source) {
  source->AddString("extensionCommandsOverlay",
      l10n_util::GetStringUTF16(IDS_EXTENSION_COMMANDS_DIALOG_TITLE));
  source->AddString("extensionCommandsEmpty",
      l10n_util::GetStringUTF16(IDS_EXTENSION_COMMANDS_EMPTY));
  source->AddString("extensionCommandsInactive",
      l10n_util::GetStringUTF16(IDS_EXTENSION_COMMANDS_INACTIVE));
  source->AddString("extensionCommandsStartTyping",
      l10n_util::GetStringUTF16(IDS_EXTENSION_TYPE_SHORTCUT));
  source->AddString("extensionCommandsDelete",
      l10n_util::GetStringUTF16(IDS_EXTENSION_DELETE_SHORTCUT));
  source->AddString("extensionCommandsGlobal",
      l10n_util::GetStringUTF16(IDS_EXTENSION_COMMANDS_GLOBAL));
  source->AddString("extensionCommandsRegular",
      l10n_util::GetStringUTF16(IDS_EXTENSION_COMMANDS_NOT_GLOBAL));
  source->AddString("ok", l10n_util::GetStringUTF16(IDS_OK));
}

void CommandHandler::RegisterMessages() {
  extension_registry_observer_.Add(ExtensionRegistry::Get(profile_));

  web_ui()->RegisterMessageCallback("extensionCommandsRequestExtensionsData",
      base::Bind(&CommandHandler::HandleRequestExtensionsData,
      base::Unretained(this)));
  web_ui()->RegisterMessageCallback("setShortcutHandlingSuspended",
      base::Bind(&CommandHandler::HandleSetShortcutHandlingSuspended,
      base::Unretained(this)));
  web_ui()->RegisterMessageCallback("setExtensionCommandShortcut",
      base::Bind(&CommandHandler::HandleSetExtensionCommandShortcut,
      base::Unretained(this)));
  web_ui()->RegisterMessageCallback("setCommandScope",
      base::Bind(&CommandHandler::HandleSetCommandScope,
      base::Unretained(this)));
}

void CommandHandler::OnExtensionLoaded(content::BrowserContext* browser_context,
                                       const Extension* extension) {
  UpdateCommandDataOnPage();
}

void CommandHandler::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  UpdateCommandDataOnPage();
}

void CommandHandler::UpdateCommandDataOnPage() {
  base::DictionaryValue results;
  GetAllCommands(&results);
  web_ui()->CallJavascriptFunction(
      "extensions.ExtensionCommandsOverlay.returnExtensionsData", results);
}

void CommandHandler::HandleRequestExtensionsData(const base::ListValue* args) {
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

void CommandHandler::HandleSetCommandScope(
    const base::ListValue* args) {
  std::string extension_id;
  std::string command_name;
  bool global;
  if (!args->GetString(0, &extension_id) ||
      !args->GetString(1, &command_name) ||
      !args->GetBoolean(2, &global)) {
    NOTREACHED();
    return;
  }

  Profile* profile = Profile::FromWebUI(web_ui());
  CommandService* command_service = CommandService::Get(profile);
  if (command_service->SetScope(extension_id, command_name, global))
    UpdateCommandDataOnPage();
}

void CommandHandler::HandleSetShortcutHandlingSuspended(
    const base::ListValue* args) {
  bool suspended;
  if (args->GetBoolean(0, &suspended)) {
    // Suspend/Resume normal shortcut handling.
    ExtensionKeybindingRegistry::SetShortcutHandlingSuspended(suspended);

    // Suspend/Resume global shortcut handling.
    ExtensionCommandsGlobalRegistry::SetShortcutHandlingSuspended(suspended);
  }
}

void CommandHandler::GetAllCommands(base::DictionaryValue* commands) {
  base::ListValue* results = new base::ListValue;

  Profile* profile = Profile::FromWebUI(web_ui());
  CommandService* command_service = CommandService::Get(profile);

  const ExtensionSet& extensions =
      ExtensionRegistry::Get(profile)->enabled_extensions();
  for (ExtensionSet::const_iterator extension = extensions.begin();
       extension != extensions.end();
       ++extension) {
    scoped_ptr<base::DictionaryValue> extension_dict(new base::DictionaryValue);
    extension_dict->SetString("name", (*extension)->name());
    extension_dict->SetString("id", (*extension)->id());

    // Add the keybindings to a list structure.
    scoped_ptr<base::ListValue> extensions_list(new base::ListValue());

    bool active = false;

    Command browser_action;
    if (command_service->GetBrowserActionCommand((*extension)->id(),
                                                 CommandService::ALL,
                                                 &browser_action,
                                                 &active)) {
      extensions_list->Append(
          browser_action.ToValue((extension->get()), active));
    }

    Command page_action;
    if (command_service->GetPageActionCommand((*extension)->id(),
                                              CommandService::ALL,
                                              &page_action,
                                              &active)) {
      extensions_list->Append(page_action.ToValue((extension->get()), active));
    }

    CommandMap named_commands;
    if (command_service->GetNamedCommands((*extension)->id(),
                                          CommandService::ALL,
                                          CommandService::ANY_SCOPE,
                                          &named_commands)) {
      for (CommandMap::const_iterator iter = named_commands.begin();
           iter != named_commands.end();
           ++iter) {
        Command command = command_service->FindCommandByName(
            (*extension)->id(), iter->second.command_name());
        ui::Accelerator shortcut_assigned = command.accelerator();

        active = (shortcut_assigned.key_code() != ui::VKEY_UNKNOWN);

        extensions_list->Append(
            iter->second.ToValue((extension->get()), active));
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

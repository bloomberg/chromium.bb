// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/command_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/api/commands/command_service_factory.h"
#include "chrome/browser/extensions/extension_keybinding_registry.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/extension_set.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

CommandHandler::CommandHandler() {
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
  localized_strings->SetString("ok", l10n_util::GetStringUTF16(IDS_OK));
}

void CommandHandler::RegisterMessages() {
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

void CommandHandler::HandleRequestExtensionsData(const ListValue* args) {
  DictionaryValue results;
  GetAllCommands(&results);
  web_ui()->CallJavascriptFunction(
      "ExtensionCommandsOverlay.returnExtensionsData", results);
}

void CommandHandler::HandleSetExtensionCommandShortcut(
    const base::ListValue* args) {
  // TODO(finnur): Implement.
}

void CommandHandler::HandleSetShortcutHandlingSuspended(const ListValue* args) {
#if !defined(OS_MACOSX)
  bool suspended;
  if (args->GetBoolean(0, &suspended))
    ExtensionKeybindingRegistry::SetShortcutHandlingSuspended(suspended);
#else
  NOTIMPLEMENTED();
#endif
}

void CommandHandler::GetAllCommands(base::DictionaryValue* commands) {
  ListValue* results = new ListValue;

  Profile* profile = Profile::FromWebUI(web_ui());
  CommandService* command_service =
      CommandServiceFactory::GetForProfile(profile);

  const ExtensionSet* extensions =
      ExtensionSystem::Get(profile)->extension_service()->extensions();
  for (ExtensionSet::const_iterator extension = extensions->begin();
       extension != extensions->end(); ++extension) {
    scoped_ptr<DictionaryValue> extension_dict(new DictionaryValue);
    extension_dict->SetString("name", (*extension)->name());
    extension_dict->SetString("id", (*extension)->id());

    // Add the keybindings to a list structure.
    scoped_ptr<ListValue> extensions_list(new ListValue());

    const extensions::Command* browser_action =
        command_service->GetBrowserActionCommand((*extension)->id(),
                                                 CommandService::ALL);
    if (browser_action) {
      extensions_list->Append(browser_action->ToValue(
          (*extension),
          command_service->IsKeybindingActive(browser_action->accelerator(),
                                              (*extension)->id(),
                                              browser_action->command_name())));
    }

    const extensions::Command* page_action =
        command_service->GetPageActionCommand((*extension)->id(),
                                              CommandService::ALL);
    if (page_action) {
      extensions_list->Append(page_action->ToValue(
          (*extension),
          command_service->IsKeybindingActive(page_action->accelerator(),
                                              (*extension)->id(),
                                              page_action->command_name())));
    }

    extensions::CommandMap named_commands =
        command_service->GetNamedCommands((*extension)->id(),
                                          CommandService::ALL);
    extensions::CommandMap::const_iterator iter = named_commands.begin();
    for (; iter != named_commands.end(); ++iter) {
      extensions_list->Append(iter->second.ToValue(
          (*extension),
          command_service->IsKeybindingActive(iter->second.accelerator(),
                                              (*extension)->id(),
                                              iter->second.command_name())));
    }

    if (!extensions_list->empty()) {
      extension_dict->Set("commands", extensions_list.release());
      results->Append(extension_dict.release());
    }
  }

  commands->Set("commands", results);
}

}  // namespace extensions

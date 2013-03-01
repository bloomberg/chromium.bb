// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/commands/command_service.h"

#include "base/lazy_instance.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/api/commands/commands.h"
#include "chrome/browser/extensions/extension_function_registry.h"
#include "chrome/browser/extensions/extension_keybinding_registry.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/prefs/pref_registry_syncable.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/api/commands/commands_handler.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"

using extensions::Extension;

namespace {

const char kExtension[] = "extension";
const char kCommandName[] = "command_name";

std::string GetPlatformKeybindingKeyForAccelerator(
    const ui::Accelerator& accelerator) {
  return extensions::Command::CommandPlatform() + ":" +
         UTF16ToUTF8(accelerator.GetShortcutText());
}

}  // namespace

namespace extensions {

// static
void CommandService::RegisterUserPrefs(PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kExtensionCommands,
                                   PrefRegistrySyncable::SYNCABLE_PREF);
}

CommandService::CommandService(Profile* profile)
    : profile_(profile) {
  (new CommandsHandler)->Register();

  ExtensionFunctionRegistry::GetInstance()->
      RegisterFunction<GetAllCommandsFunction>();

  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_INSTALLED,
      content::Source<Profile>(profile));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
      content::Source<Profile>(profile));
}

CommandService::~CommandService() {
}

static base::LazyInstance<ProfileKeyedAPIFactory<CommandService> >
g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<CommandService>* CommandService::GetFactoryInstance() {
  return &g_factory.Get();
}

// static
CommandService* CommandService::Get(Profile* profile) {
  return ProfileKeyedAPIFactory<CommandService>::GetForProfile(profile);
}

bool CommandService::GetBrowserActionCommand(
    const std::string& extension_id,
    QueryType type,
    extensions::Command* command,
    bool* active) {
  return GetExtensionActionCommand(
      extension_id, type, command, active, BROWSER_ACTION);
}

bool CommandService::GetPageActionCommand(
    const std::string& extension_id,
    QueryType type,
    extensions::Command* command,
    bool* active) {
  return GetExtensionActionCommand(
      extension_id, type, command, active, PAGE_ACTION);
}

bool CommandService::GetScriptBadgeCommand(
    const std::string& extension_id,
    QueryType type,
    extensions::Command* command,
    bool* active) {
  return GetExtensionActionCommand(
      extension_id, type, command, active, SCRIPT_BADGE);
}

bool CommandService::GetNamedCommands(const std::string& extension_id,
                                      QueryType type,
                                      extensions::CommandMap* command_map) {
  const ExtensionSet* extensions =
      ExtensionSystem::Get(profile_)->extension_service()->extensions();
  const Extension* extension = extensions->GetByID(extension_id);
  CHECK(extension);

  command_map->clear();
  const extensions::CommandMap* commands =
      CommandsInfo::GetNamedCommands(extension);
  if (!commands)
    return false;

  extensions::CommandMap::const_iterator iter = commands->begin();
  for (; iter != commands->end(); ++iter) {
    ui::Accelerator shortcut_assigned =
        FindShortcutForCommand(extension_id, iter->second.command_name());

    if (type == ACTIVE_ONLY && shortcut_assigned.key_code() == ui::VKEY_UNKNOWN)
      continue;

    extensions::Command command = iter->second;
    if (shortcut_assigned.key_code() != ui::VKEY_UNKNOWN)
      command.set_accelerator(shortcut_assigned);

    (*command_map)[iter->second.command_name()] = command;
  }

  return true;
}

bool CommandService::AddKeybindingPref(
    const ui::Accelerator& accelerator,
    std::string extension_id,
    std::string command_name,
    bool allow_overrides) {
  if (accelerator.key_code() == ui::VKEY_UNKNOWN)
    return false;

  DictionaryPrefUpdate updater(profile_->GetPrefs(),
                               prefs::kExtensionCommands);
  DictionaryValue* bindings = updater.Get();

  std::string key = GetPlatformKeybindingKeyForAccelerator(accelerator);

  if (!allow_overrides && bindings->HasKey(key))
    return false;  // Already taken.

  DictionaryValue* keybinding = new DictionaryValue();
  keybinding->SetString(kExtension, extension_id);
  keybinding->SetString(kCommandName, command_name);

  bindings->Set(key, keybinding);

  std::pair<const std::string, const std::string> details =
      std::make_pair(extension_id, command_name);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_COMMAND_ADDED,
      content::Source<Profile>(profile_),
      content::Details<
          std::pair<const std::string, const std::string> >(&details));

  return true;
}

void CommandService::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_INSTALLED:
      AssignInitialKeybindings(
          content::Details<const Extension>(details).ptr());
      break;
    case chrome::NOTIFICATION_EXTENSION_UNINSTALLED:
      RemoveKeybindingPrefs(
          content::Details<const Extension>(details).ptr()->id(),
          std::string());
      break;
    default:
      NOTREACHED();
      break;
  }
}

void CommandService::UpdateKeybindingPrefs(const std::string& extension_id,
                                           const std::string& command_name,
                                           const std::string& keystroke) {
  // The extension command might be assigned another shortcut. Remove that
  // shortcut before proceeding.
  RemoveKeybindingPrefs(extension_id, command_name);

  ui::Accelerator accelerator = Command::StringToAccelerator(keystroke);
  AddKeybindingPref(accelerator, extension_id, command_name, true);
}

ui::Accelerator CommandService::FindShortcutForCommand(
    const std::string& extension_id, const std::string& command) {
  const DictionaryValue* bindings =
      profile_->GetPrefs()->GetDictionary(prefs::kExtensionCommands);
  for (DictionaryValue::key_iterator it = bindings->begin_keys();
       it != bindings->end_keys(); ++it) {
    const DictionaryValue* item = NULL;
    bindings->GetDictionary(*it, &item);

    std::string extension;
    item->GetString(kExtension, &extension);
    if (extension != extension_id)
      continue;
    std::string command_name;
    item->GetString(kCommandName, &command_name);
    if (command != command_name)
      continue;

    std::string shortcut = *it;
    if (StartsWithASCII(shortcut, Command::CommandPlatform() + ":", true))
      shortcut = shortcut.substr(Command::CommandPlatform().length() + 1);

    return Command::StringToAccelerator(shortcut);
  }

  return ui::Accelerator();
}

void CommandService::AssignInitialKeybindings(const Extension* extension) {
  const extensions::CommandMap* commands =
      CommandsInfo::GetNamedCommands(extension);
  if (!commands)
    return;

  extensions::CommandMap::const_iterator iter = commands->begin();
  for (; iter != commands->end(); ++iter) {
    AddKeybindingPref(iter->second.accelerator(),
                      extension->id(),
                      iter->second.command_name(),
                      false);  // Overwriting not allowed.
  }

  const extensions::Command* browser_action_command =
      CommandsInfo::GetBrowserActionCommand(extension);
  if (browser_action_command) {
    AddKeybindingPref(browser_action_command->accelerator(),
                      extension->id(),
                      browser_action_command->command_name(),
                      false);  // Overwriting not allowed.
  }

  const extensions::Command* page_action_command =
      CommandsInfo::GetPageActionCommand(extension);
  if (page_action_command) {
    AddKeybindingPref(page_action_command->accelerator(),
                      extension->id(),
                      page_action_command->command_name(),
                      false);  // Overwriting not allowed.
  }

  const extensions::Command* script_badge_command =
      CommandsInfo::GetScriptBadgeCommand(extension);
  if (script_badge_command) {
    AddKeybindingPref(script_badge_command->accelerator(),
                      extension->id(),
                      script_badge_command->command_name(),
                      false);  // Overwriting not allowed.
  }
}

void CommandService::RemoveKeybindingPrefs(const std::string& extension_id,
                                           const std::string& command_name) {
  DictionaryPrefUpdate updater(profile_->GetPrefs(),
                               prefs::kExtensionCommands);
  DictionaryValue* bindings = updater.Get();

  typedef std::vector<std::string> KeysToRemove;
  KeysToRemove keys_to_remove;
  for (DictionaryValue::key_iterator it = bindings->begin_keys();
       it != bindings->end_keys(); ++it) {
    std::string key = *it;
    DictionaryValue* item = NULL;
    bindings->GetDictionary(key, &item);

    std::string extension;
    item->GetString(kExtension, &extension);

    if (extension == extension_id) {
      // If |command_name| is specified, delete only that command. Otherwise,
      // delete all commands.
      if (!command_name.empty()) {
        std::string command;
        item->GetString(kCommandName, &command);
        if (command_name != command)
          continue;
      }

      keys_to_remove.push_back(key);
    }
  }

  for (KeysToRemove::const_iterator it = keys_to_remove.begin();
       it != keys_to_remove.end(); ++it) {
    std::string key = *it;
    bindings->Remove(key, NULL);

    std::pair<const std::string, const std::string> details =
        std::make_pair(extension_id, command_name);
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_EXTENSION_COMMAND_REMOVED,
        content::Source<Profile>(profile_),
        content::Details<
            std::pair<const std::string, const std::string> >(&details));
  }
}

bool CommandService::GetExtensionActionCommand(
    const std::string& extension_id,
    QueryType query_type,
    extensions::Command* command,
    bool* active,
    ExtensionActionType action_type) {
  ExtensionService* service =
      ExtensionSystem::Get(profile_)->extension_service();
  if (!service)
    return false;  // Can happen in tests.
  const ExtensionSet* extensions = service->extensions();
  const Extension* extension = extensions->GetByID(extension_id);
  CHECK(extension);

  if (active)
    *active = false;

  const extensions::Command* requested_command = NULL;
  switch (action_type) {
    case BROWSER_ACTION:
      requested_command = CommandsInfo::GetBrowserActionCommand(extension);
      break;
    case PAGE_ACTION:
      requested_command = CommandsInfo::GetPageActionCommand(extension);
      break;
    case SCRIPT_BADGE:
      requested_command = CommandsInfo::GetScriptBadgeCommand(extension);
      break;
  }
  if (!requested_command)
    return false;

  ui::Accelerator shortcut_assigned =
      FindShortcutForCommand(extension_id, requested_command->command_name());

  if (active)
    *active = (shortcut_assigned.key_code() != ui::VKEY_UNKNOWN);

  if (query_type == ACTIVE_ONLY &&
      shortcut_assigned.key_code() == ui::VKEY_UNKNOWN)
    return false;

  *command = *requested_command;
  if (shortcut_assigned.key_code() != ui::VKEY_UNKNOWN)
    command->set_accelerator(shortcut_assigned);

  return true;
}

}  // namespace extensions

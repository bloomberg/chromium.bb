// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/commands/extension_command_service.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_keybinding_registry.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"

namespace {

const char kExtension[] = "extension";
const char kCommandName[] = "command_name";

std::string GetPlatformKeybindingKeyForAccelerator(
    const ui::Accelerator& accelerator) {
  return extensions::Command::CommandPlatform() + ":" +
         UTF16ToUTF8(accelerator.GetShortcutText());
}

}  // namespace

// static
void ExtensionCommandService::RegisterUserPrefs(
  PrefService* user_prefs) {
      user_prefs->RegisterDictionaryPref(prefs::kExtensionKeybindings,
                                         PrefService::SYNCABLE_PREF);
}

ExtensionCommandService::ExtensionCommandService(
    Profile* profile)
    : profile_(profile) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_INSTALLED,
      content::Source<Profile>(profile));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
      content::Source<Profile>(profile));
}

ExtensionCommandService::~ExtensionCommandService() {
}

const extensions::Command*
    ExtensionCommandService::GetActiveBrowserActionCommand(
        const std::string& extension_id) {
  const ExtensionSet* extensions =
      ExtensionSystem::Get(profile_)->extension_service()->extensions();
  const Extension* extension = extensions->GetByID(extension_id);
  CHECK(extension);

  const extensions::Command* command = extension->browser_action_command();
  if (!command)
    return NULL;
  if (!IsKeybindingActive(command->accelerator(),
                          extension_id,
                          command->command_name())) {
    return NULL;
  }

  return command;
}

const extensions::Command* ExtensionCommandService::GetActivePageActionCommand(
    const std::string& extension_id) {
  const ExtensionSet* extensions =
      ExtensionSystem::Get(profile_)->extension_service()->extensions();
  const Extension* extension = extensions->GetByID(extension_id);
  CHECK(extension);

  const extensions::Command* command = extension->page_action_command();
  if (!command)
    return NULL;
  if (!IsKeybindingActive(command->accelerator(),
                          extension_id,
                          command->command_name())) {
    return NULL;
  }

  return command;
}

extensions::CommandMap ExtensionCommandService::GetActiveNamedCommands(
    const std::string& extension_id) {
  const ExtensionSet* extensions =
      ExtensionSystem::Get(profile_)->extension_service()->extensions();
  const Extension* extension = extensions->GetByID(extension_id);
  CHECK(extension);

  extensions::CommandMap result;
  const extensions::CommandMap& commands = extension->named_commands();
  if (commands.empty())
    return result;

  extensions::CommandMap::const_iterator iter = commands.begin();
  for (; iter != commands.end(); ++iter) {
    if (!IsKeybindingActive(iter->second.accelerator(),
                            extension_id,
                            iter->second.command_name())) {
      continue;
    }

    result[iter->second.command_name()] = iter->second;
  }

  return result;
}

bool ExtensionCommandService::IsKeybindingActive(
    const ui::Accelerator& accelerator,
    std::string extension_id,
    std::string command_name) {
  CHECK(!extension_id.empty());
  CHECK(!command_name.empty());

  std::string key = GetPlatformKeybindingKeyForAccelerator(accelerator);
  const DictionaryValue* bindings =
      profile_->GetPrefs()->GetDictionary(prefs::kExtensionKeybindings);
  if (!bindings->HasKey(key))
    return false;

  DictionaryValue* value = NULL;
  if (!bindings->GetDictionary(key, &value))
    return false;

  std::string id;
  if (!value->GetString(kExtension, &id) || id != extension_id)
    return false;  // Not active for this extension.

  std::string command;
  if (!value->GetString(kCommandName, &command) || command != command_name)
    return false;  // Not active for this command.

  return true;  // We found a match, this one is active.
}

bool ExtensionCommandService::AddKeybindingPref(
    const ui::Accelerator& accelerator,
    std::string extension_id,
    std::string command_name,
    bool allow_overrides) {
  DictionaryPrefUpdate updater(profile_->GetPrefs(),
                               prefs::kExtensionKeybindings);
  DictionaryValue* bindings = updater.Get();

  std::string key = GetPlatformKeybindingKeyForAccelerator(accelerator);
  if (bindings->HasKey(key) && !allow_overrides)
    return false;  // Already taken.

  DictionaryValue* keybinding = new DictionaryValue();
  keybinding->SetString(kExtension, extension_id);
  keybinding->SetString(kCommandName, command_name);

  bindings->Set(key, keybinding);
  return true;
}

void ExtensionCommandService::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_INSTALLED:
      AssignInitialKeybindings(
          content::Details<const Extension>(details).ptr());
      break;
    case chrome::NOTIFICATION_EXTENSION_UNINSTALLED:
      RemoveKeybindingPrefs(*content::Details<std::string>(details).ptr());
      break;
    default:
      NOTREACHED();
      break;
  }
}

void ExtensionCommandService::AssignInitialKeybindings(
    const Extension* extension) {
  const extensions::CommandMap& commands = extension->named_commands();
  extensions::CommandMap::const_iterator iter = commands.begin();
  for (; iter != commands.end(); ++iter) {
    AddKeybindingPref(iter->second.accelerator(),
                      extension->id(),
                      iter->second.command_name(),
                      false);  // Overwriting not allowed.
  }

  const extensions::Command* browser_action_command =
      extension->browser_action_command();
  if (browser_action_command) {
    AddKeybindingPref(browser_action_command->accelerator(),
                      extension->id(),
                      browser_action_command->command_name(),
                      false);  // Overwriting not allowed.
  }

  const extensions::Command* page_action_command =
      extension->page_action_command();
  if (page_action_command) {
    AddKeybindingPref(page_action_command->accelerator(),
                      extension->id(),
                      page_action_command->command_name(),
                      false);  // Overwriting not allowed.
  }
}

void ExtensionCommandService::RemoveKeybindingPrefs(std::string extension_id) {
  DictionaryPrefUpdate updater(profile_->GetPrefs(),
                               prefs::kExtensionKeybindings);
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
    if (extension == extension_id)
      keys_to_remove.push_back(key);
  }

  for (KeysToRemove::const_iterator it = keys_to_remove.begin();
       it != keys_to_remove.end(); ++it) {
    bindings->Remove(*it, NULL);
  }
}

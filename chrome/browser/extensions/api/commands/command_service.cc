// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/commands/command_service.h"

#include <vector>

#include "base/lazy_instance.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/commands/commands.h"
#include "chrome/browser/extensions/extension_commands_global_registry.h"
#include "chrome/browser/extensions/extension_keybinding_registry.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/accelerator_utils.h"
#include "chrome/common/extensions/api/commands/commands_handler.h"
#include "chrome/common/extensions/manifest_handlers/settings_overrides_handler.h"
#include "chrome/common/extensions/manifest_handlers/ui_overrides_handler.h"
#include "chrome/common/pref_names.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/permissions_data.h"

using extensions::Extension;
using extensions::ExtensionPrefs;
using extensions::SettingsOverrides;
using extensions::UIOverrides;

namespace {

const char kExtension[] = "extension";
const char kCommandName[] = "command_name";
const char kGlobal[] = "global";

// A preference that indicates that the initial keybindings for the given
// extension have been set.
const char kInitialBindingsHaveBeenAssigned[] = "initial_keybindings_set";

std::string GetPlatformKeybindingKeyForAccelerator(
    const ui::Accelerator& accelerator, const std::string extension_id) {
  std::string key = extensions::Command::CommandPlatform() + ":" +
                    extensions::Command::AcceleratorToString(accelerator);

  // Media keys have a 1-to-many relationship with targets, unlike regular
  // shortcut (1-to-1 relationship). That means two or more extensions can
  // register for the same media key so the extension ID needs to be added to
  // the key to make sure the key is unique.
  if (extensions::Command::IsMediaKey(accelerator))
    key += ":" + extension_id;

  return key;
}

bool IsForCurrentPlatform(const std::string& key) {
  return StartsWithASCII(
      key, extensions::Command::CommandPlatform() + ":", true);
}

void SetInitialBindingsHaveBeenAssigned(
    ExtensionPrefs* prefs, const std::string& extension_id) {
  prefs->UpdateExtensionPref(extension_id, kInitialBindingsHaveBeenAssigned,
                             new base::FundamentalValue(true));
}

bool InitialBindingsHaveBeenAssigned(
    const ExtensionPrefs* prefs, const std::string& extension_id) {
  bool assigned = false;
  if (!prefs || !prefs->ReadPrefAsBoolean(extension_id,
                                          kInitialBindingsHaveBeenAssigned,
                                          &assigned))
    return false;

  return assigned;
}

// Checks if |extension| is permitted to automatically assign the |accelerator|
// key.
bool CanAutoAssign(const ui::Accelerator& accelerator,
                   const Extension* extension,
                   Profile* profile,
                   bool is_named_command,
                   bool is_global) {
  // Media Keys are non-exclusive, so allow auto-assigning them.
  if (extensions::Command::IsMediaKey(accelerator))
    return true;

  if (is_global) {
    if (!is_named_command)
      return false;  // Browser and page actions are not global in nature.

    // Global shortcuts are restricted to (Ctrl|Command)+Shift+[0-9].
#if defined OS_MACOSX
    if (!accelerator.IsCmdDown())
      return false;
#else
    if (!accelerator.IsCtrlDown())
      return false;
#endif
    if (!accelerator.IsShiftDown())
      return false;
    return (accelerator.key_code() >= ui::VKEY_0 &&
            accelerator.key_code() <= ui::VKEY_9);
  } else {
    // Not a global command, check if Chrome shortcut and whether
    // we can override it.
    if (accelerator ==
        chrome::GetPrimaryChromeAcceleratorForCommandId(IDC_BOOKMARK_PAGE) &&
        extensions::CommandService::RemovesBookmarkShortcut(extension)) {
      // If this check fails it either means we have an API to override a
      // key that isn't a ChromeAccelerator (and the API can therefore be
      // deprecated) or the IsChromeAccelerator isn't consistently
      // returning true for all accelerators.
      DCHECK(chrome::IsChromeAccelerator(accelerator, profile));
      return true;
    }

    return !chrome::IsChromeAccelerator(accelerator, profile);
  }
}

}  // namespace

namespace extensions {

// static
void CommandService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(
      prefs::kExtensionCommands,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

CommandService::CommandService(content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)) {
  ExtensionFunctionRegistry::GetInstance()->
      RegisterFunction<GetAllCommandsFunction>();

  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_INSTALLED,
                 content::Source<Profile>(profile_));
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
                 content::Source<Profile>(profile_));
}

CommandService::~CommandService() {
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<CommandService> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<CommandService>*
CommandService::GetFactoryInstance() {
  return g_factory.Pointer();
}

// static
CommandService* CommandService::Get(content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<CommandService>::Get(context);
}

// static
bool CommandService::RemovesBookmarkShortcut(
    const extensions::Extension* extension) {
  const UIOverrides* ui_overrides = UIOverrides::Get(extension);
  const SettingsOverrides* settings_overrides =
      SettingsOverrides::Get(extension);

  return ((settings_overrides &&
           SettingsOverrides::RemovesBookmarkShortcut(*settings_overrides)) ||
          (ui_overrides &&
           UIOverrides::RemovesBookmarkShortcut(*ui_overrides))) &&
      (extensions::PermissionsData::HasAPIPermission(
          extension,
          extensions::APIPermission::kBookmarkManagerPrivate) ||
       extensions::FeatureSwitch::enable_override_bookmarks_ui()->
           IsEnabled());
}

// static
bool CommandService::RemovesBookmarkOpenPagesShortcut(
    const extensions::Extension* extension) {
  const UIOverrides* ui_overrides = UIOverrides::Get(extension);
  const SettingsOverrides* settings_overrides =
      SettingsOverrides::Get(extension);

  return ((settings_overrides &&
           SettingsOverrides::RemovesBookmarkOpenPagesShortcut(
               *settings_overrides)) ||
          (ui_overrides &&
           UIOverrides::RemovesBookmarkOpenPagesShortcut(*ui_overrides))) &&
      (extensions::PermissionsData::HasAPIPermission(
          extension,
          extensions::APIPermission::kBookmarkManagerPrivate) ||
       extensions::FeatureSwitch::enable_override_bookmarks_ui()->
           IsEnabled());
}

bool CommandService::GetBrowserActionCommand(const std::string& extension_id,
                                             QueryType type,
                                             extensions::Command* command,
                                             bool* active) const {
  return GetExtensionActionCommand(
      extension_id, type, command, active, BROWSER_ACTION);
}

bool CommandService::GetPageActionCommand(const std::string& extension_id,
                                          QueryType type,
                                          extensions::Command* command,
                                          bool* active) const {
  return GetExtensionActionCommand(
      extension_id, type, command, active, PAGE_ACTION);
}

bool CommandService::GetNamedCommands(
    const std::string& extension_id,
    QueryType type,
    CommandScope scope,
    extensions::CommandMap* command_map) const {
  const ExtensionSet& extensions =
      ExtensionRegistry::Get(profile_)->enabled_extensions();
  const Extension* extension = extensions.GetByID(extension_id);
  CHECK(extension);

  command_map->clear();
  const extensions::CommandMap* commands =
      CommandsInfo::GetNamedCommands(extension);
  if (!commands)
    return false;

  extensions::CommandMap::const_iterator iter = commands->begin();
  for (; iter != commands->end(); ++iter) {
    // Look up to see if the user has overridden how the command should work.
    extensions::Command saved_command =
        FindCommandByName(extension_id, iter->second.command_name());
    ui::Accelerator shortcut_assigned = saved_command.accelerator();

    if (type == ACTIVE_ONLY && shortcut_assigned.key_code() == ui::VKEY_UNKNOWN)
      continue;

    extensions::Command command = iter->second;
    if (scope != ANY_SCOPE && ((scope == GLOBAL) != saved_command.global()))
      continue;

    if (shortcut_assigned.key_code() != ui::VKEY_UNKNOWN)
      command.set_accelerator(shortcut_assigned);
    command.set_global(saved_command.global());

    (*command_map)[iter->second.command_name()] = command;
  }

  return true;
}

bool CommandService::AddKeybindingPref(
    const ui::Accelerator& accelerator,
    std::string extension_id,
    std::string command_name,
    bool allow_overrides,
    bool global) {
  if (accelerator.key_code() == ui::VKEY_UNKNOWN)
    return false;

  // Media Keys are allowed to be used by named command only.
  DCHECK(!Command::IsMediaKey(accelerator) ||
         (command_name != manifest_values::kPageActionCommandEvent &&
          command_name != manifest_values::kBrowserActionCommandEvent));

  DictionaryPrefUpdate updater(profile_->GetPrefs(),
                               prefs::kExtensionCommands);
  base::DictionaryValue* bindings = updater.Get();

  std::string key = GetPlatformKeybindingKeyForAccelerator(accelerator,
                                                           extension_id);

  if (bindings->HasKey(key)) {
    if (!allow_overrides)
      return false;  // Already taken.

    // If the shortcut has been assigned to another command, it should be
    // removed before overriding, so that |ExtensionKeybindingRegistry| can get
    // a chance to do clean-up.
    const base::DictionaryValue* item = NULL;
    bindings->GetDictionary(key, &item);
    std::string old_extension_id;
    std::string old_command_name;
    item->GetString(kExtension, &old_extension_id);
    item->GetString(kCommandName, &old_command_name);
    RemoveKeybindingPrefs(old_extension_id, old_command_name);
  }

  base::DictionaryValue* keybinding = new base::DictionaryValue();
  keybinding->SetString(kExtension, extension_id);
  keybinding->SetString(kCommandName, command_name);
  keybinding->SetBoolean(kGlobal, global);

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
          content::Details<const InstalledExtensionInfo>(details)->extension);
      break;
    case chrome::NOTIFICATION_EXTENSION_UNINSTALLED:
      RemoveKeybindingPrefs(
          content::Details<const Extension>(details)->id(),
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
  extensions::Command command = FindCommandByName(extension_id, command_name);

  // The extension command might be assigned another shortcut. Remove that
  // shortcut before proceeding.
  RemoveKeybindingPrefs(extension_id, command_name);

  ui::Accelerator accelerator =
      Command::StringToAccelerator(keystroke, command_name);
  AddKeybindingPref(accelerator, extension_id, command_name,
                    true, command.global());
}

bool CommandService::SetScope(const std::string& extension_id,
                              const std::string& command_name,
                              bool global) {
  extensions::Command command = FindCommandByName(extension_id, command_name);
  if (global == command.global())
    return false;

  // Pre-existing shortcuts must be removed before proceeding because the
  // handlers for global and non-global extensions are not one and the same.
  RemoveKeybindingPrefs(extension_id, command_name);
  AddKeybindingPref(command.accelerator(), extension_id,
                    command_name, true, global);
  return true;
}

Command CommandService::FindCommandByName(const std::string& extension_id,
                                          const std::string& command) const {
  const base::DictionaryValue* bindings =
      profile_->GetPrefs()->GetDictionary(prefs::kExtensionCommands);
  for (base::DictionaryValue::Iterator it(*bindings); !it.IsAtEnd();
       it.Advance()) {
    const base::DictionaryValue* item = NULL;
    it.value().GetAsDictionary(&item);

    std::string extension;
    item->GetString(kExtension, &extension);
    if (extension != extension_id)
      continue;
    std::string command_name;
    item->GetString(kCommandName, &command_name);
    if (command != command_name)
      continue;
    // Format stored in Preferences is: "Platform:Shortcut[:ExtensionId]".
    std::string shortcut = it.key();
    if (!IsForCurrentPlatform(shortcut))
      continue;
    bool global = false;
    if (FeatureSwitch::global_commands()->IsEnabled())
      item->GetBoolean(kGlobal, &global);

    std::vector<std::string> tokens;
    base::SplitString(shortcut, ':', &tokens);
    CHECK(tokens.size() >= 2);
    shortcut = tokens[1];

    return Command(command_name, base::string16(), shortcut, global);
  }

  return Command();
}

bool CommandService::GetBoundExtensionCommand(
    const std::string& extension_id,
    const ui::Accelerator& accelerator,
    extensions::Command* command,
    ExtensionCommandType* command_type) const {
  const ExtensionSet& extensions =
      ExtensionRegistry::Get(profile_)->enabled_extensions();
  const Extension* extension = extensions.GetByID(extension_id);
  CHECK(extension);

  extensions::Command prospective_command;
  extensions::CommandMap command_map;
  bool active = false;
  if (GetBrowserActionCommand(extension_id,
                              extensions::CommandService::ACTIVE_ONLY,
                              &prospective_command,
                              &active) && active &&
          accelerator == prospective_command.accelerator()) {
    if (command)
      *command = prospective_command;
    if (command_type)
      *command_type = BROWSER_ACTION;
    return true;
  } else if (GetPageActionCommand(extension_id,
                                  extensions::CommandService::ACTIVE_ONLY,
                                  &prospective_command,
                                  &active) && active &&
                 accelerator == prospective_command.accelerator()) {
    if (command)
      *command = prospective_command;
    if (command_type)
      *command_type = PAGE_ACTION;
    return true;
  } else if (GetNamedCommands(extension_id,
                              extensions::CommandService::ACTIVE_ONLY,
                              extensions::CommandService::REGULAR,
                              &command_map)) {
    for (extensions::CommandMap::const_iterator it = command_map.begin();
         it != command_map.end(); ++it) {
      if (accelerator == it->second.accelerator()) {
        if (command)
          *command = it->second;
        if (command_type)
          *command_type = NAMED;
        return true;
      }
    }
  }
  return false;
}

bool CommandService::OverridesBookmarkShortcut(
    const extensions::Extension* extension) const {
  return RemovesBookmarkShortcut(extension) &&
      GetBoundExtensionCommand(
          extension->id(),
          chrome::GetPrimaryChromeAcceleratorForCommandId(IDC_BOOKMARK_PAGE),
          NULL,
          NULL);
}

void CommandService::AssignInitialKeybindings(const Extension* extension) {
  const extensions::CommandMap* commands =
      CommandsInfo::GetNamedCommands(extension);
  if (!commands)
    return;

  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile_);
  if (InitialBindingsHaveBeenAssigned(extension_prefs, extension->id()))
    return;
  SetInitialBindingsHaveBeenAssigned(extension_prefs, extension->id());

  extensions::CommandMap::const_iterator iter = commands->begin();
  for (; iter != commands->end(); ++iter) {
    const extensions::Command command = iter->second;
    if (CanAutoAssign(command.accelerator(),
                      extension,
                      profile_,
                      true,  // Is a named command.
                      command.global())) {
      AddKeybindingPref(command.accelerator(),
                        extension->id(),
                        command.command_name(),
                        false,  // Overwriting not allowed.
                        command.global());
    }
  }

  const extensions::Command* browser_action_command =
      CommandsInfo::GetBrowserActionCommand(extension);
  if (browser_action_command &&
      CanAutoAssign(browser_action_command->accelerator(),
                    extension,
                    profile_,
                    false,     // Not a named command.
                    false)) {  // Not global.
    AddKeybindingPref(browser_action_command->accelerator(),
                      extension->id(),
                      browser_action_command->command_name(),
                      false,   // Overwriting not allowed.
                      false);  // Not global.
  }

  const extensions::Command* page_action_command =
      CommandsInfo::GetPageActionCommand(extension);
  if (page_action_command &&
      CanAutoAssign(page_action_command->accelerator(),
                    extension,
                    profile_,
                    false,     // Not a named command.
                    false)) {  // Not global.
    AddKeybindingPref(page_action_command->accelerator(),
                      extension->id(),
                      page_action_command->command_name(),
                      false,   // Overwriting not allowed.
                      false);  // Not global.
  }
}

void CommandService::RemoveKeybindingPrefs(const std::string& extension_id,
                                           const std::string& command_name) {
  DictionaryPrefUpdate updater(profile_->GetPrefs(),
                               prefs::kExtensionCommands);
  base::DictionaryValue* bindings = updater.Get();

  typedef std::vector<std::string> KeysToRemove;
  KeysToRemove keys_to_remove;
  for (base::DictionaryValue::Iterator it(*bindings); !it.IsAtEnd();
       it.Advance()) {
    // Removal of keybinding preference should be limited to current platform.
    if (!IsForCurrentPlatform(it.key()))
      continue;

    const base::DictionaryValue* item = NULL;
    it.value().GetAsDictionary(&item);

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

      keys_to_remove.push_back(it.key());
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
    ExtensionCommandType action_type) const {
  const ExtensionSet& extensions =
      ExtensionRegistry::Get(profile_)->enabled_extensions();
  const Extension* extension = extensions.GetByID(extension_id);
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
    case NAMED:
      NOTREACHED();
      return false;
  }
  if (!requested_command)
    return false;

  // Look up to see if the user has overridden how the command should work.
  extensions::Command saved_command =
      FindCommandByName(extension_id, requested_command->command_name());
  ui::Accelerator shortcut_assigned = saved_command.accelerator();

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

template <>
void
BrowserContextKeyedAPIFactory<CommandService>::DeclareFactoryDependencies() {
  DependsOn(ExtensionCommandsGlobalRegistry::GetFactoryInstance());
}

}  // namespace extensions

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_commands_global_registry.h"

#include "base/lazy_instance.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/extension_keybinding_registry.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/global_shortcut_listener.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"

namespace extensions {

ExtensionCommandsGlobalRegistry::ExtensionCommandsGlobalRegistry(
    Profile* profile)
    : ExtensionKeybindingRegistry(
          profile, ExtensionKeybindingRegistry::ALL_EXTENSIONS, NULL),
      profile_(profile) {
  Init();
}

ExtensionCommandsGlobalRegistry::~ExtensionCommandsGlobalRegistry() {
  EventTargets::const_iterator iter;
  for (iter = event_targets_.begin(); iter != event_targets_.end(); ++iter) {
    GlobalShortcutListener::GetInstance()->UnregisterAccelerator(
        iter->first, this);
  }
}

static base::LazyInstance<
    ProfileKeyedAPIFactory<ExtensionCommandsGlobalRegistry> >
g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<ExtensionCommandsGlobalRegistry>*
ExtensionCommandsGlobalRegistry::GetFactoryInstance() {
  return &g_factory.Get();
}

// static
ExtensionCommandsGlobalRegistry*
ExtensionCommandsGlobalRegistry::Get(Profile* profile) {
  return ProfileKeyedAPIFactory<
      ExtensionCommandsGlobalRegistry>::GetForProfile(profile);
}


void ExtensionCommandsGlobalRegistry::AddExtensionKeybinding(
    const extensions::Extension* extension,
    const std::string& command_name) {
  // This object only handles named commands, not browser/page actions.
  if (ShouldIgnoreCommand(command_name))
    return;

  extensions::CommandService* command_service =
      extensions::CommandService::Get(profile_);
  // Add all the active global keybindings, if any.
  extensions::CommandMap commands;
  if (!command_service->GetNamedCommands(
          extension->id(),
          extensions::CommandService::ACTIVE_ONLY,
          extensions::CommandService::GLOBAL,
          &commands))
    return;

  extensions::CommandMap::const_iterator iter = commands.begin();
  for (; iter != commands.end(); ++iter) {
    if (!command_name.empty() && (iter->second.command_name() != command_name))
      continue;

    VLOG(0) << "Adding global keybinding for " << extension->name().c_str()
            << " " << command_name.c_str()
            << " key: " << iter->second.accelerator().GetShortcutText();

    event_targets_[iter->second.accelerator()] =
        std::make_pair(extension->id(), iter->second.command_name());

    GlobalShortcutListener::GetInstance()->RegisterAccelerator(
      iter->second.accelerator(), this);
  }
}

void ExtensionCommandsGlobalRegistry::RemoveExtensionKeybindingImpl(
    const ui::Accelerator& accelerator,
    const std::string& command_name) {
  VLOG(0) << "Removing keybinding for " << command_name.c_str();

  GlobalShortcutListener::GetInstance()->UnregisterAccelerator(
      accelerator, this);
}

void ExtensionCommandsGlobalRegistry::OnKeyPressed(
    const ui::Accelerator& accelerator) {
  EventTargets::iterator it = event_targets_.find(accelerator);
  if (it == event_targets_.end()) {
    NOTREACHED();  // Shouldn't get this event for something not registered.
    return;
  }

  CommandExecuted(it->second.first, it->second.second);
}

}  // namespace extensions

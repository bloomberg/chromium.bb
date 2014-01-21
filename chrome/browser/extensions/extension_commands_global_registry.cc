// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_commands_global_registry.h"

#include "base/lazy_instance.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/global_shortcut_listener.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/common/extension.h"

namespace extensions {

ExtensionCommandsGlobalRegistry::ExtensionCommandsGlobalRegistry(
    Profile* profile)
    : ExtensionKeybindingRegistry(
          profile, ExtensionKeybindingRegistry::ALL_EXTENSIONS, NULL),
      profile_(profile) {
  Init();
}

ExtensionCommandsGlobalRegistry::~ExtensionCommandsGlobalRegistry() {
  for (EventTargets::const_iterator iter = event_targets_.begin();
       iter != event_targets_.end(); ++iter) {
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
  return g_factory.Pointer();
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
    const ui::Accelerator& accelerator = iter->second.accelerator();

    VLOG(0) << "Adding global keybinding for " << extension->name().c_str()
            << " " << command_name.c_str()
            << " key: " << accelerator.GetShortcutText();

    if (event_targets_.find(accelerator) == event_targets_.end()) {
      if (!GlobalShortcutListener::GetInstance()->RegisterAccelerator(
              accelerator, this))
        continue;
    }

    event_targets_[accelerator].push_back(
        std::make_pair(extension->id(), iter->second.command_name()));
    // Shortcuts except media keys have only one target in the list. See comment
    // about |event_targets_|.
    if (!extensions::CommandService::IsMediaKey(accelerator))
      DCHECK_EQ(1u, event_targets_[accelerator].size());
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
  ExtensionKeybindingRegistry::NotifyEventTargets(accelerator);
}

}  // namespace extensions

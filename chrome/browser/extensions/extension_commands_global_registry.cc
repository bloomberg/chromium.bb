// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_commands_global_registry.h"

#include "base/lazy_instance.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/global_shortcut_listener.h"
#include "extensions/common/extension.h"

namespace extensions {

ExtensionCommandsGlobalRegistry::ExtensionCommandsGlobalRegistry(
    content::BrowserContext* context)
    : ExtensionKeybindingRegistry(context,
                                  ExtensionKeybindingRegistry::ALL_EXTENSIONS,
                                  NULL),
      browser_context_(context),
      registry_for_active_window_(NULL) {
  Init();
}

ExtensionCommandsGlobalRegistry::~ExtensionCommandsGlobalRegistry() {
  if (!IsEventTargetsEmpty()) {
    GlobalShortcutListener* global_shortcut_listener =
        GlobalShortcutListener::GetInstance();

    // Resume GlobalShortcutListener before we clean up if the shortcut handling
    // is currently suspended.
    if (global_shortcut_listener->IsShortcutHandlingSuspended())
      global_shortcut_listener->SetShortcutHandlingSuspended(false);

    global_shortcut_listener->UnregisterAccelerators(this);
  }
}

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<ExtensionCommandsGlobalRegistry> > g_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<ExtensionCommandsGlobalRegistry>*
ExtensionCommandsGlobalRegistry::GetFactoryInstance() {
  return g_factory.Pointer();
}

// static
ExtensionCommandsGlobalRegistry* ExtensionCommandsGlobalRegistry::Get(
    content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<ExtensionCommandsGlobalRegistry>::Get(
      context);
}

// static
void ExtensionCommandsGlobalRegistry::SetShortcutHandlingSuspended(
    bool suspended) {
  GlobalShortcutListener::GetInstance()->SetShortcutHandlingSuspended(
      suspended);
}

bool ExtensionCommandsGlobalRegistry::IsRegistered(
    const ui::Accelerator& accelerator) {
  return (registry_for_active_window() &&
          registry_for_active_window()->IsAcceleratorRegistered(accelerator)) ||
         IsAcceleratorRegistered(accelerator);
}

void ExtensionCommandsGlobalRegistry::AddExtensionKeybinding(
    const extensions::Extension* extension,
    const std::string& command_name) {
  // This object only handles named commands, not browser/page actions.
  if (ShouldIgnoreCommand(command_name))
    return;

  extensions::CommandService* command_service =
      extensions::CommandService::Get(browser_context_);
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

    if (!IsAcceleratorRegistered(accelerator)) {
      if (!GlobalShortcutListener::GetInstance()->RegisterAccelerator(
              accelerator, this))
        continue;
    }

    AddEventTarget(accelerator, extension->id(), iter->second.command_name());
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

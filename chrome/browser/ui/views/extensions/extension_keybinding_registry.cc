// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_keybinding_registry.h"

#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension_constants.h"
#include "ui/views/focus/focus_manager.h"

ExtensionKeybindingRegistry::ExtensionKeybindingRegistry(
    Profile* profile, views::FocusManager* focus_manager)
    : profile_(profile),
      focus_manager_(focus_manager) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(profile->GetOriginalProfile()));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile->GetOriginalProfile()));

  ExtensionService* service = profile->GetExtensionService();
  if (!service)
    return;  // ExtensionService can be null during testing.

  const ExtensionSet* extensions = service->extensions();

  ExtensionSet::const_iterator iter = extensions->begin();
  for (; iter != extensions->end(); ++iter)
    AddExtensionKeybinding(*iter);
}

ExtensionKeybindingRegistry::~ExtensionKeybindingRegistry() {
  EventTargets::const_iterator iter;
  for (iter = event_targets_.begin(); iter != event_targets_.end(); ++iter)
    focus_manager_->UnregisterAccelerator(iter->first, this);
}

void ExtensionKeybindingRegistry::AddExtensionKeybinding(
    const Extension* extension) {
  // Add all the keybindings (except pageAction and browserAction, which are
  // handled elsewhere).
  const std::vector<Extension::ExtensionKeybinding> commands =
      extension->keybindings();
  for (size_t i = 0; i < commands.size(); ++i) {
    if (ShouldIgnoreCommand(commands[i].command_name()))
      continue;

    event_targets_[commands[i].accelerator()] =
        std::make_pair(extension->id(), commands[i].command_name());
    focus_manager_->RegisterAccelerator(
        commands[i].accelerator(), ui::AcceleratorManager::kHighPriority, this);
  }
}

void ExtensionKeybindingRegistry::RemoveExtensionKeybinding(
    const Extension* extension) {
  EventTargets::const_iterator iter;
  for (iter = event_targets_.begin(); iter != event_targets_.end(); ++iter) {
    if (iter->second.first != extension->id())
      continue;  // Not the extension we asked for.

    focus_manager_->UnregisterAccelerator(iter->first, this);
  }
}

bool ExtensionKeybindingRegistry::ShouldIgnoreCommand(
    const std::string& command) const {
  return command == extension_manifest_values::kPageActionKeybindingEvent ||
         command == extension_manifest_values::kBrowserActionKeybindingEvent;
}

bool ExtensionKeybindingRegistry::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  ExtensionService* service = profile_->GetExtensionService();

  EventTargets::iterator it = event_targets_.find(accelerator);
  if (it == event_targets_.end()) {
    NOTREACHED();  // Shouldn't get this event for something not registered.
    return false;
  }

  service->browser_event_router()->CommandExecuted(
      profile_, it->second.first, it->second.second);

  return true;
}

bool ExtensionKeybindingRegistry::CanHandleAccelerators() const {
  return true;
}

void ExtensionKeybindingRegistry::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_LOADED:
      AddExtensionKeybinding(
          content::Details<const Extension>(details).ptr());
      break;
    case chrome::NOTIFICATION_EXTENSION_UNLOADED:
      RemoveExtensionKeybinding(
          content::Details<UnloadedExtensionInfo>(details)->extension);
      break;
    default:
      NOTREACHED();
      break;
  }
}

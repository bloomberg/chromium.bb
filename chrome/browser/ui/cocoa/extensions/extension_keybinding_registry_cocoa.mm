// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/extensions/extension_keybinding_registry_cocoa.h"

#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/api/commands/command_service_factory.h"
#include "chrome/browser/extensions/browser_event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/notification_service.h"

namespace values = extension_manifest_values;

// static
void extensions::ExtensionKeybindingRegistry::SetShortcutHandlingSuspended(
    bool suspended) {
  ExtensionKeybindingRegistryCocoa::set_shortcut_handling_suspended(suspended);
}

bool ExtensionKeybindingRegistryCocoa::shortcut_handling_suspended_ = false;

ExtensionKeybindingRegistryCocoa::ExtensionKeybindingRegistryCocoa(
    Profile* profile,
    gfx::NativeWindow window,
    ExtensionFilter extension_filter)
    : ExtensionKeybindingRegistry(profile, extension_filter),
      profile_(profile),
      window_(window) {
  Init();
}

ExtensionKeybindingRegistryCocoa::~ExtensionKeybindingRegistryCocoa() {
}

bool ExtensionKeybindingRegistryCocoa::ProcessKeyEvent(
    const content::NativeWebKeyboardEvent& event) {
  if (shortcut_handling_suspended_)
    return false;

  ui::Accelerator accelerator(
      static_cast<ui::KeyboardCode>(event.windowsKeyCode),
      content::GetModifiersFromNativeWebKeyboardEvent(event));
  EventTargets::iterator it = event_targets_.find(accelerator);
  if (it == event_targets_.end())
    return false;

  std::string extension_id = it->second.first;
  std::string command_name = it->second.second;
  int type = 0;
  if (command_name == values::kPageActionCommandEvent) {
    type = chrome::NOTIFICATION_EXTENSION_COMMAND_PAGE_ACTION_MAC;
  } else if (command_name == values::kBrowserActionCommandEvent) {
    type = chrome::NOTIFICATION_EXTENSION_COMMAND_BROWSER_ACTION_MAC;
  } else if (command_name == values::kScriptBadgeCommandEvent) {
    type = chrome::NOTIFICATION_EXTENSION_COMMAND_SCRIPT_BADGE_MAC;
  } else {
    // Not handled by using notifications. Route it through the Browser Event
    // Router.
    ExtensionService* service = profile_->GetExtensionService();
    service->browser_event_router()->CommandExecuted(
        profile_, extension_id, command_name);
    return true;
  }

  std::pair<const std::string, gfx::NativeWindow> details =
      std::make_pair(extension_id, window_);
  content::NotificationService::current()->Notify(
      type,
      content::Source<Profile>(profile_),
      content::Details<
          std::pair<const std::string, gfx::NativeWindow> >(&details));
  return true;
}

void ExtensionKeybindingRegistryCocoa::AddExtensionKeybinding(
    const extensions::Extension* extension,
    const std::string& command_name) {
  extensions::CommandService* command_service =
      extensions::CommandServiceFactory::GetForProfile(profile_);
  extensions::CommandMap commands;
  command_service->GetNamedCommands(
          extension->id(),
          extensions::CommandService::ACTIVE_ONLY,
          &commands);

  for (extensions::CommandMap::const_iterator iter = commands.begin();
       iter != commands.end(); ++iter) {
    if (!command_name.empty() && (iter->second.command_name() != command_name))
      continue;

    ui::Accelerator accelerator(iter->second.accelerator());
    event_targets_[accelerator] =
        std::make_pair(extension->id(), iter->second.command_name());
  }

  // Mac implemenetation behaves like GTK with regards to what is kept in the
  // event_targets_ map, because both GTK and Mac need to keep track of Browser
  // and Page actions, as well as Script Badges.
  extensions::Command browser_action;
  if (command_service->GetBrowserActionCommand(
          extension->id(),
          extensions::CommandService::ACTIVE_ONLY,
          &browser_action,
          NULL)) {
    ui::Accelerator accelerator(browser_action.accelerator());
    event_targets_[accelerator] =
        std::make_pair(extension->id(), browser_action.command_name());
  }

  // Add the Page Action (if any).
  extensions::Command page_action;
  if (command_service->GetPageActionCommand(
          extension->id(),
          extensions::CommandService::ACTIVE_ONLY,
          &page_action,
          NULL)) {
    ui::Accelerator accelerator(page_action.accelerator());
    event_targets_[accelerator] =
        std::make_pair(extension->id(), page_action.command_name());
  }

  // Add the Script Badge (if any).
  extensions::Command script_badge;
  if (command_service->GetScriptBadgeCommand(
          extension->id(),
          extensions::CommandService::ACTIVE_ONLY,
          &script_badge,
          NULL)) {
    ui::Accelerator accelerator(script_badge.accelerator());
    event_targets_[accelerator] =
        std::make_pair(extension->id(), script_badge.command_name());
  }
}

void ExtensionKeybindingRegistryCocoa::RemoveExtensionKeybinding(
    const extensions::Extension* extension,
    const std::string& command_name) {
  EventTargets::iterator iter = event_targets_.begin();
  while (iter != event_targets_.end()) {
    EventTargets::iterator old = iter++;
    if (old->second.first == extension->id() &&
        (command_name.empty() || (old->second.second == command_name)))
      event_targets_.erase(old);
  }
}

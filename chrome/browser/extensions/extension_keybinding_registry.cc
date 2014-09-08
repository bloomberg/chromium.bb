// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_keybinding_registry.h"

#include "base/values.h"
#include "chrome/browser/extensions/active_tab_permission_granter.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/command.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_constants.h"

namespace {
const char kOnCommandEventName[] = "commands.onCommand";
}  // namespace

namespace extensions {

ExtensionKeybindingRegistry::ExtensionKeybindingRegistry(
    content::BrowserContext* context,
    ExtensionFilter extension_filter,
    Delegate* delegate)
    : browser_context_(context),
      extension_filter_(extension_filter),
      delegate_(delegate),
      extension_registry_observer_(this) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(browser_context_));

  Profile* profile = Profile::FromBrowserContext(browser_context_);
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_COMMAND_ADDED,
                 content::Source<Profile>(profile->GetOriginalProfile()));
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_COMMAND_REMOVED,
                 content::Source<Profile>(profile->GetOriginalProfile()));
}

ExtensionKeybindingRegistry::~ExtensionKeybindingRegistry() {
}

void ExtensionKeybindingRegistry::RemoveExtensionKeybinding(
    const Extension* extension,
    const std::string& command_name) {
  EventTargets::iterator it = event_targets_.begin();
  while (it != event_targets_.end()) {
    TargetList& target_list = it->second;
    TargetList::iterator target = target_list.begin();
    while (target != target_list.end()) {
      if (target->first == extension->id() &&
          (command_name.empty() || command_name == target->second))
        target = target_list.erase(target);
      else
        target++;
    }

    EventTargets::iterator old = it++;
    if (target_list.empty()) {
      // Let each platform-specific implementation get a chance to clean up.
      RemoveExtensionKeybindingImpl(old->first, command_name);
      event_targets_.erase(old);

      // If a specific command_name was requested, it has now been deleted so no
      // further work is required.
      if (!command_name.empty())
        break;
    }
  }
}

void ExtensionKeybindingRegistry::Init() {
  ExtensionService* service =
      ExtensionSystem::Get(browser_context_)->extension_service();
  if (!service)
    return;  // ExtensionService can be null during testing.

  const ExtensionSet* extensions = service->extensions();
  ExtensionSet::const_iterator iter = extensions->begin();
  for (; iter != extensions->end(); ++iter)
    if (ExtensionMatchesFilter(iter->get()))
      AddExtensionKeybinding(iter->get(), std::string());
}

bool ExtensionKeybindingRegistry::ShouldIgnoreCommand(
    const std::string& command) const {
  return command == manifest_values::kPageActionCommandEvent ||
         command == manifest_values::kBrowserActionCommandEvent;
}

bool ExtensionKeybindingRegistry::NotifyEventTargets(
    const ui::Accelerator& accelerator) {
  return ExecuteCommands(accelerator, std::string());
}

void ExtensionKeybindingRegistry::CommandExecuted(
    const std::string& extension_id, const std::string& command) {
  ExtensionService* service =
      ExtensionSystem::Get(browser_context_)->extension_service();

  const Extension* extension = service->extensions()->GetByID(extension_id);
  if (!extension)
    return;

  // Grant before sending the event so that the permission is granted before
  // the extension acts on the command. NOTE: The Global Commands handler does
  // not set the delegate as it deals only with named commands (not page/browser
  // actions that are associated with the current page directly).
  ActiveTabPermissionGranter* granter =
      delegate_ ? delegate_->GetActiveTabPermissionGranter() : NULL;
  if (granter)
    granter->GrantIfRequested(extension);

  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Append(new base::StringValue(command));

  scoped_ptr<Event> event(new Event(kOnCommandEventName, args.Pass()));
  event->restrict_to_browser_context = browser_context_;
  event->user_gesture = EventRouter::USER_GESTURE_ENABLED;
  EventRouter::Get(browser_context_)
      ->DispatchEventToExtension(extension_id, event.Pass());
}

bool ExtensionKeybindingRegistry::IsAcceleratorRegistered(
    const ui::Accelerator& accelerator) const {
  return event_targets_.find(accelerator) != event_targets_.end();
}

void ExtensionKeybindingRegistry::AddEventTarget(
    const ui::Accelerator& accelerator,
    const std::string& extension_id,
    const std::string& command_name) {
  event_targets_[accelerator].push_back(
      std::make_pair(extension_id, command_name));
  // Shortcuts except media keys have only one target in the list. See comment
  // about |event_targets_|.
  if (!extensions::Command::IsMediaKey(accelerator))
    DCHECK_EQ(1u, event_targets_[accelerator].size());
}

bool ExtensionKeybindingRegistry::GetFirstTarget(
    const ui::Accelerator& accelerator,
    std::string* extension_id,
    std::string* command_name) const {
  EventTargets::const_iterator targets = event_targets_.find(accelerator);
  if (targets == event_targets_.end())
    return false;

  DCHECK(!targets->second.empty());
  TargetList::const_iterator first_target = targets->second.begin();
  *extension_id = first_target->first;
  *command_name = first_target->second;
  return true;
}

bool ExtensionKeybindingRegistry::IsEventTargetsEmpty() const {
  return event_targets_.empty();
}

void ExtensionKeybindingRegistry::ExecuteCommand(
    const std::string& extension_id,
    const ui::Accelerator& accelerator) {
  ExecuteCommands(accelerator, extension_id);
}

void ExtensionKeybindingRegistry::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  if (ExtensionMatchesFilter(extension))
    AddExtensionKeybinding(extension, std::string());
}

void ExtensionKeybindingRegistry::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  if (ExtensionMatchesFilter(extension))
    RemoveExtensionKeybinding(extension, std::string());
}

void ExtensionKeybindingRegistry::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case extensions::NOTIFICATION_EXTENSION_COMMAND_ADDED:
    case extensions::NOTIFICATION_EXTENSION_COMMAND_REMOVED: {
      std::pair<const std::string, const std::string>* payload =
          content::Details<std::pair<const std::string, const std::string> >(
              details).ptr();

      const Extension* extension = ExtensionSystem::Get(browser_context_)
                                       ->extension_service()
                                       ->extensions()
                                       ->GetByID(payload->first);
      // During install and uninstall the extension won't be found. We'll catch
      // those events above, with the LOADED/UNLOADED, so we ignore this event.
      if (!extension)
        return;

      if (ExtensionMatchesFilter(extension)) {
        if (type == extensions::NOTIFICATION_EXTENSION_COMMAND_ADDED)
          AddExtensionKeybinding(extension, payload->second);
        else
          RemoveExtensionKeybinding(extension, payload->second);
      }
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

bool ExtensionKeybindingRegistry::ExtensionMatchesFilter(
    const extensions::Extension* extension)
{
  switch (extension_filter_) {
    case ALL_EXTENSIONS:
      return true;
    case PLATFORM_APPS_ONLY:
      return extension->is_platform_app();
    default:
      NOTREACHED();
  }
  return false;
}

bool ExtensionKeybindingRegistry::ExecuteCommands(
    const ui::Accelerator& accelerator,
    const std::string& extension_id) {
  EventTargets::iterator targets = event_targets_.find(accelerator);
  if (targets == event_targets_.end() || targets->second.empty())
    return false;

  bool executed = false;
  for (TargetList::const_iterator it = targets->second.begin();
       it != targets->second.end(); it++) {
    if (!extensions::EventRouter::Get(browser_context_)
        ->ExtensionHasEventListener(it->first, kOnCommandEventName))
      continue;

    if (extension_id.empty() || it->first == extension_id) {
      CommandExecuted(it->first, it->second);
      executed = true;
    }
  }

  return executed;
}

}  // namespace extensions

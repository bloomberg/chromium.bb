// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_keybinding_registry.h"

#include "base/values.h"
#include "chrome/browser/extensions/active_tab_permission_granter.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/extension_set.h"

namespace extensions {

ExtensionKeybindingRegistry::ExtensionKeybindingRegistry(
    Profile* profile, ExtensionFilter extension_filter, Delegate* delegate)
    : profile_(profile),
      extension_filter_(extension_filter),
      delegate_(delegate) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(profile->GetOriginalProfile()));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile->GetOriginalProfile()));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_COMMAND_ADDED,
                 content::Source<Profile>(profile->GetOriginalProfile()));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_COMMAND_REMOVED,
                 content::Source<Profile>(profile->GetOriginalProfile()));
}

ExtensionKeybindingRegistry::~ExtensionKeybindingRegistry() {
}

void ExtensionKeybindingRegistry::Init() {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  if (!service)
    return;  // ExtensionService can be null during testing.

  const ExtensionSet* extensions = service->extensions();
  ExtensionSet::const_iterator iter = extensions->begin();
  for (; iter != extensions->end(); ++iter)
    if (ExtensionMatchesFilter(*iter))
      AddExtensionKeybinding(*iter, std::string());
}

bool ExtensionKeybindingRegistry::ShouldIgnoreCommand(
    const std::string& command) const {
  return command == extension_manifest_values::kPageActionCommandEvent ||
         command == extension_manifest_values::kBrowserActionCommandEvent ||
         command == extension_manifest_values::kScriptBadgeCommandEvent;
}

void ExtensionKeybindingRegistry::CommandExecuted(
    const std::string& extension_id, const std::string& command) {
  ExtensionService* service =
      ExtensionSystem::Get(profile_)->extension_service();

  const Extension* extension = service->extensions()->GetByID(extension_id);
  if (!extension)
    return;

  // Grant before sending the event so that the permission is granted before
  // the extension acts on the command.
  ActiveTabPermissionGranter* granter =
      delegate_->GetActiveTabPermissionGranter();
  if (granter)
    granter->GrantIfRequested(extension);

  scoped_ptr<ListValue> args(new ListValue());
  args->Append(Value::CreateStringValue(command));

  scoped_ptr<Event> event(new Event("commands.onCommand", args.Pass()));
  event->restrict_to_profile = profile_;
  event->user_gesture = EventRouter::USER_GESTURE_ENABLED;
  ExtensionSystem::Get(profile_)->event_router()->
      DispatchEventToExtension(extension_id, event.Pass());
}

void ExtensionKeybindingRegistry::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_LOADED: {
      const extensions::Extension* extension =
          content::Details<const extensions::Extension>(details).ptr();
      if (ExtensionMatchesFilter(extension))
        AddExtensionKeybinding(extension, std::string());
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      const extensions::Extension* extension =
          content::Details<UnloadedExtensionInfo>(details)->extension;
      if (ExtensionMatchesFilter(extension))
        RemoveExtensionKeybinding(extension, std::string());
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_COMMAND_ADDED:
    case chrome::NOTIFICATION_EXTENSION_COMMAND_REMOVED: {
      std::pair<const std::string, const std::string>* payload =
          content::Details<std::pair<const std::string, const std::string> >(
              details).ptr();

      const extensions::Extension* extension =
          ExtensionSystem::Get(profile_)->extension_service()->
              extensions()->GetByID(payload->first);
      // During install and uninstall the extension won't be found. We'll catch
      // those events above, with the LOADED/UNLOADED, so we ignore this event.
      if (!extension)
        return;

      if (ExtensionMatchesFilter(extension)) {
        if (type == chrome::NOTIFICATION_EXTENSION_COMMAND_ADDED)
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

}  // namespace extensions

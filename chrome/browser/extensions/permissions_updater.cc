// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/permissions_updater.h"

#include "base/json/json_writer.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/permissions/permissions_api_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/permissions.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/permissions/permissions_data.h"

using content::RenderProcessHost;
using extensions::permissions_api_helpers::PackPermissionSet;

namespace extensions {

namespace permissions = api::permissions;

PermissionsUpdater::PermissionsUpdater(Profile* profile)
    : profile_(profile) {}

PermissionsUpdater::~PermissionsUpdater() {}

void PermissionsUpdater::AddPermissions(
    const Extension* extension, const PermissionSet* permissions) {
  scoped_refptr<const PermissionSet> existing(
      extension->GetActivePermissions());
  scoped_refptr<PermissionSet> total(
      PermissionSet::CreateUnion(existing.get(), permissions));
  scoped_refptr<PermissionSet> added(
      PermissionSet::CreateDifference(total.get(), existing.get()));

  UpdateActivePermissions(extension, total.get());

  // Update the granted permissions so we don't auto-disable the extension.
  GrantActivePermissions(extension);

  NotifyPermissionsUpdated(ADDED, extension, added.get());
}

void PermissionsUpdater::RemovePermissions(
    const Extension* extension, const PermissionSet* permissions) {
  scoped_refptr<const PermissionSet> existing(
      extension->GetActivePermissions());
  scoped_refptr<PermissionSet> total(
      PermissionSet::CreateDifference(existing.get(), permissions));
  scoped_refptr<PermissionSet> removed(
      PermissionSet::CreateDifference(existing.get(), total.get()));

  // We update the active permissions, and not the granted permissions, because
  // the extension, not the user, removed the permissions. This allows the
  // extension to add them again without prompting the user.
  UpdateActivePermissions(extension, total.get());

  NotifyPermissionsUpdated(REMOVED, extension, removed.get());
}

void PermissionsUpdater::GrantActivePermissions(const Extension* extension) {
  CHECK(extension);

  // We only maintain the granted permissions prefs for INTERNAL and LOAD
  // extensions.
  if (!Manifest::IsUnpackedLocation(extension->location()) &&
      extension->location() != Manifest::INTERNAL)
    return;

  ExtensionPrefs::Get(profile_)->AddGrantedPermissions(
      extension->id(), extension->GetActivePermissions().get());
}

void PermissionsUpdater::UpdateActivePermissions(
    const Extension* extension, const PermissionSet* permissions) {
  ExtensionPrefs::Get(profile_)->SetActivePermissions(
      extension->id(), permissions);
  PermissionsData::SetActivePermissions(extension, permissions);
}

void PermissionsUpdater::DispatchEvent(
    const std::string& extension_id,
    const char* event_name,
    const PermissionSet* changed_permissions) {
  if (!profile_ || !EventRouter::Get(profile_))
    return;

  scoped_ptr<base::ListValue> value(new base::ListValue());
  scoped_ptr<api::permissions::Permissions> permissions =
      PackPermissionSet(changed_permissions);
  value->Append(permissions->ToValue().release());
  scoped_ptr<Event> event(new Event(event_name, value.Pass()));
  event->restrict_to_browser_context = profile_;
  EventRouter::Get(profile_)
      ->DispatchEventToExtension(extension_id, event.Pass());
}

void PermissionsUpdater::NotifyPermissionsUpdated(
    EventType event_type,
    const Extension* extension,
    const PermissionSet* changed) {
  if (!changed || changed->IsEmpty())
    return;

  UpdatedExtensionPermissionsInfo::Reason reason;
  const char* event_name = NULL;

  if (event_type == REMOVED) {
    reason = UpdatedExtensionPermissionsInfo::REMOVED;
    event_name = permissions::OnRemoved::kEventName;
  } else {
    CHECK_EQ(ADDED, event_type);
    reason = UpdatedExtensionPermissionsInfo::ADDED;
    event_name = permissions::OnAdded::kEventName;
  }

  // Notify other APIs or interested parties.
  UpdatedExtensionPermissionsInfo info = UpdatedExtensionPermissionsInfo(
      extension, changed, reason);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_PERMISSIONS_UPDATED,
      content::Source<Profile>(profile_),
      content::Details<UpdatedExtensionPermissionsInfo>(&info));

  // Send the new permissions to the renderers.
  for (RenderProcessHost::iterator i(RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    RenderProcessHost* host = i.GetCurrentValue();
    Profile* profile = Profile::FromBrowserContext(host->GetBrowserContext());
    if (profile_->IsSameProfile(profile)) {
      ExtensionMsg_UpdatePermissions_Params info;
      info.reason_id = static_cast<int>(reason);
      info.extension_id = extension->id();
      info.apis = changed->apis();
      info.manifest_permissions = changed->manifest_permissions();
      info.explicit_hosts = changed->explicit_hosts();
      info.scriptable_hosts = changed->scriptable_hosts();
      host->Send(new ExtensionMsg_UpdatePermissions(info));
    }
  }

  // Trigger the onAdded and onRemoved events in the extension.
  DispatchEvent(extension->id(), event_name, changed);
}

}  // namespace extensions

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
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"

using content::RenderProcessHost;
using extensions::permissions_api_helpers::PackPermissionSet;

namespace extensions {

namespace permissions = api::permissions;

PermissionsUpdater::PermissionsUpdater(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
}

PermissionsUpdater::~PermissionsUpdater() {}

void PermissionsUpdater::AddPermissions(
    const Extension* extension, const PermissionSet* permissions) {
  scoped_refptr<const PermissionSet> existing(
      extension->permissions_data()->active_permissions());
  scoped_refptr<PermissionSet> total(
      PermissionSet::CreateUnion(existing.get(), permissions));
  scoped_refptr<PermissionSet> added(
      PermissionSet::CreateDifference(total.get(), existing.get()));

  SetActivePermissions(extension, total.get());

  // Update the granted permissions so we don't auto-disable the extension.
  GrantActivePermissions(extension);

  NotifyPermissionsUpdated(ADDED, extension, added.get());
}

void PermissionsUpdater::RemovePermissions(
    const Extension* extension, const PermissionSet* permissions) {
  scoped_refptr<const PermissionSet> existing(
      extension->permissions_data()->active_permissions());
  scoped_refptr<PermissionSet> total(
      PermissionSet::CreateDifference(existing.get(), permissions));
  scoped_refptr<PermissionSet> removed(
      PermissionSet::CreateDifference(existing.get(), total.get()));

  // We update the active permissions, and not the granted permissions, because
  // the extension, not the user, removed the permissions. This allows the
  // extension to add them again without prompting the user.
  SetActivePermissions(extension, total.get());

  NotifyPermissionsUpdated(REMOVED, extension, removed.get());
}

void PermissionsUpdater::GrantActivePermissions(const Extension* extension) {
  CHECK(extension);

  // We only maintain the granted permissions prefs for INTERNAL and LOAD
  // extensions.
  if (!Manifest::IsUnpackedLocation(extension->location()) &&
      extension->location() != Manifest::INTERNAL)
    return;

  ExtensionPrefs::Get(browser_context_)->AddGrantedPermissions(
      extension->id(),
      extension->permissions_data()->active_permissions().get());
}

void PermissionsUpdater::InitializeActivePermissions(
    const Extension* extension) {
  // If the extension has used the optional permissions API, it will have a
  // custom set of active permissions defined in the extension prefs. Here,
  // we update the extension's active permissions based on the prefs.
  scoped_refptr<PermissionSet> active_permissions =
      ExtensionPrefs::Get(browser_context_)->GetActivePermissions(
          extension->id());
  if (!active_permissions)
    return;

  // We restrict the active permissions to be within the bounds defined in the
  // extension's manifest.
  //  a) active permissions must be a subset of optional + default permissions
  //  b) active permissions must contains all default permissions
  scoped_refptr<PermissionSet> total_permissions = PermissionSet::CreateUnion(
      PermissionsParser::GetRequiredPermissions(extension),
      PermissionsParser::GetOptionalPermissions(extension));

  // Make sure the active permissions contain no more than optional + default.
  scoped_refptr<PermissionSet> adjusted_active =
      PermissionSet::CreateIntersection(total_permissions, active_permissions);

  // Make sure the active permissions contain the default permissions.
  adjusted_active = PermissionSet::CreateUnion(
      PermissionsParser::GetRequiredPermissions(extension),
      adjusted_active);

  SetActivePermissions(extension, adjusted_active);
}

void PermissionsUpdater::SetActivePermissions(
    const Extension* extension, const PermissionSet* permissions) {
  ExtensionPrefs::Get(browser_context_)->SetActivePermissions(
      extension->id(), permissions);
  extension->permissions_data()->SetActivePermissions(permissions);
}

void PermissionsUpdater::DispatchEvent(
    const std::string& extension_id,
    const char* event_name,
    const PermissionSet* changed_permissions) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (!event_router)
    return;

  scoped_ptr<base::ListValue> value(new base::ListValue());
  scoped_ptr<api::permissions::Permissions> permissions =
      PackPermissionSet(changed_permissions);
  value->Append(permissions->ToValue().release());
  scoped_ptr<Event> event(new Event(event_name, value.Pass()));
  event->restrict_to_browser_context = browser_context_;
  event_router->DispatchEventToExtension(extension_id, event.Pass());
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
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_PERMISSIONS_UPDATED,
      content::Source<Profile>(profile),
      content::Details<UpdatedExtensionPermissionsInfo>(&info));

  ExtensionMsg_UpdatePermissions_Params params;
  params.reason_id = static_cast<int>(reason);
  params.extension_id = extension->id();
  params.apis = changed->apis();
  params.manifest_permissions = changed->manifest_permissions();
  params.explicit_hosts = changed->explicit_hosts();
  params.scriptable_hosts = changed->scriptable_hosts();

  // Send the new permissions to the renderers.
  for (RenderProcessHost::iterator i(RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    RenderProcessHost* host = i.GetCurrentValue();
    if (profile->IsSameProfile(
            Profile::FromBrowserContext(host->GetBrowserContext()))) {
      host->Send(new ExtensionMsg_UpdatePermissions(params));
    }
  }

  // Trigger the onAdded and onRemoved events in the extension.
  DispatchEvent(extension->id(), event_name, changed);
}

}  // namespace extensions

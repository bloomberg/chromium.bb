// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/permissions_updater.h"

#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/permissions/permissions_api_helpers.h"
#include "chrome/browser/extensions/scripting_permissions_modifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/permissions.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"

using content::RenderProcessHost;
using extensions::permissions_api_helpers::PackPermissionSet;

namespace extensions {

namespace permissions = api::permissions;

namespace {

// Returns a PermissionSet that has the active permissions of the extension,
// bounded to its current manifest.
scoped_ptr<const PermissionSet> GetBoundedActivePermissions(
    const Extension* extension,
    const PermissionSet* active_permissions) {
  // If the extension has used the optional permissions API, it will have a
  // custom set of active permissions defined in the extension prefs. Here,
  // we update the extension's active permissions based on the prefs.
  if (!active_permissions)
    return extension->permissions_data()->active_permissions().Clone();

  const PermissionSet& required_permissions =
      PermissionsParser::GetRequiredPermissions(extension);

  // We restrict the active permissions to be within the bounds defined in the
  // extension's manifest.
  //  a) active permissions must be a subset of optional + default permissions
  //  b) active permissions must contains all default permissions
  scoped_ptr<const PermissionSet> total_permissions =
      PermissionSet::CreateUnion(
          required_permissions,
          PermissionsParser::GetOptionalPermissions(extension));

  // Make sure the active permissions contain no more than optional + default.
  scoped_ptr<const PermissionSet> adjusted_active =
      PermissionSet::CreateIntersection(*total_permissions,
                                        *active_permissions);

  // Make sure the active permissions contain the default permissions.
  adjusted_active =
      PermissionSet::CreateUnion(required_permissions, *adjusted_active);

  return adjusted_active;
}

}  // namespace

PermissionsUpdater::PermissionsUpdater(content::BrowserContext* browser_context)
    : browser_context_(browser_context), init_flag_(INIT_FLAG_NONE) {
}

PermissionsUpdater::PermissionsUpdater(content::BrowserContext* browser_context,
                                       InitFlag init_flag)
    : browser_context_(browser_context), init_flag_(init_flag) {
}

PermissionsUpdater::~PermissionsUpdater() {}

void PermissionsUpdater::AddPermissions(const Extension* extension,
                                        const PermissionSet& permissions) {
  const PermissionSet& active =
      extension->permissions_data()->active_permissions();
  scoped_ptr<const PermissionSet> total =
      PermissionSet::CreateUnion(active, permissions);
  scoped_ptr<const PermissionSet> added =
      PermissionSet::CreateDifference(*total, active);

  scoped_ptr<const PermissionSet> new_withheld =
      PermissionSet::CreateDifference(
          extension->permissions_data()->withheld_permissions(), permissions);
  SetPermissions(extension, total.Pass(), new_withheld.Pass());

  // Update the granted permissions so we don't auto-disable the extension.
  GrantActivePermissions(extension);

  NotifyPermissionsUpdated(ADDED, extension, *added);
}

void PermissionsUpdater::RemovePermissions(const Extension* extension,
                                           const PermissionSet& to_remove,
                                           RemoveType remove_type) {
  // We should only be revoking revokable permissions.
  CHECK(GetRevokablePermissions(extension)->Contains(to_remove));

  const PermissionSet& active =
      extension->permissions_data()->active_permissions();
  scoped_ptr<const PermissionSet> remaining =
      PermissionSet::CreateDifference(active, to_remove);

  // Move any granted permissions that were in the withheld set back to the
  // withheld set so they can be added back later.
  // Any revoked permission that isn't from the optional permissions can only
  // be a withheld permission.
  scoped_ptr<const PermissionSet> removed_withheld =
      PermissionSet::CreateDifference(
          to_remove, PermissionsParser::GetOptionalPermissions(extension));
  scoped_ptr<const PermissionSet> withheld = PermissionSet::CreateUnion(
      *removed_withheld, extension->permissions_data()->withheld_permissions());

  SetPermissions(extension, remaining.Pass(), withheld.Pass());

  // We might not want to revoke the granted permissions because the extension,
  // not the user, removed the permissions. This allows the extension to add
  // them again without prompting the user.
  if (remove_type == REMOVE_HARD) {
    ExtensionPrefs::Get(browser_context_)
        ->RemoveGrantedPermissions(extension->id(), to_remove);
  }

  NotifyPermissionsUpdated(REMOVED, extension, to_remove);
}

void PermissionsUpdater::RemovePermissionsUnsafe(
    const Extension* extension,
    const PermissionSet& to_remove) {
  const PermissionSet& active =
      extension->permissions_data()->active_permissions();
  scoped_ptr<const PermissionSet> total =
      PermissionSet::CreateDifference(active, to_remove);
  // |successfully_removed| might not equal |to_remove| if |to_remove| contains
  // permissions the extension didn't have.
  scoped_ptr<const PermissionSet> successfully_removed =
      PermissionSet::CreateDifference(active, *total);

  SetPermissions(extension, total.Pass(), nullptr);
  NotifyPermissionsUpdated(REMOVED, extension, *successfully_removed);
}

scoped_ptr<const PermissionSet> PermissionsUpdater::GetRevokablePermissions(
    const Extension* extension) const {
  // The user can revoke any permissions they granted. In other words, any
  // permissions the extension didn't start with can be revoked.
  const PermissionSet& required =
      PermissionsParser::GetRequiredPermissions(extension);
  scoped_ptr<const PermissionSet> granted;
  scoped_ptr<const PermissionSet> withheld;
  ScriptingPermissionsModifier(browser_context_, make_scoped_refptr(extension))
      .WithholdPermissions(required, &granted, &withheld, true);
  return PermissionSet::CreateDifference(
      extension->permissions_data()->active_permissions(), *granted);
}

void PermissionsUpdater::GrantActivePermissions(const Extension* extension) {
  CHECK(extension);

  ExtensionPrefs::Get(browser_context_)
      ->AddGrantedPermissions(
          extension->id(), extension->permissions_data()->active_permissions());
}

void PermissionsUpdater::InitializePermissions(const Extension* extension) {
  scoped_ptr<const PermissionSet> bounded_wrapper;
  const PermissionSet* bounded_active = nullptr;
  // If |extension| is a transient dummy extension, we do not want to look for
  // it in preferences.
  if (init_flag_ & INIT_FLAG_TRANSIENT) {
    bounded_active = &extension->permissions_data()->active_permissions();
  } else {
    scoped_ptr<const PermissionSet> active_permissions =
        ExtensionPrefs::Get(browser_context_)
            ->GetActivePermissions(extension->id());
    bounded_wrapper =
        GetBoundedActivePermissions(extension, active_permissions.get());
    bounded_active = bounded_wrapper.get();
  }

  scoped_ptr<const PermissionSet> granted_permissions;
  scoped_ptr<const PermissionSet> withheld_permissions;
  ScriptingPermissionsModifier(browser_context_, make_scoped_refptr(extension))
      .WithholdPermissions(*bounded_active, &granted_permissions,
                           &withheld_permissions,
                           (init_flag_ & INIT_FLAG_TRANSIENT) != 0);

  SetPermissions(extension, granted_permissions.Pass(),
                 withheld_permissions.Pass());
}

void PermissionsUpdater::SetPermissions(
    const Extension* extension,
    scoped_ptr<const PermissionSet> active,
    scoped_ptr<const PermissionSet> withheld) {
  DCHECK(active);
  const PermissionSet& active_weak = *active;
  if (withheld) {
    extension->permissions_data()->SetPermissions(active.Pass(),
                                                  withheld.Pass());
  } else {
    extension->permissions_data()->SetActivePermissions(active.Pass());
  }

  if ((init_flag_ & INIT_FLAG_TRANSIENT) == 0) {
    ExtensionPrefs::Get(browser_context_)
        ->SetActivePermissions(extension->id(), active_weak);
  }
}

void PermissionsUpdater::DispatchEvent(
    const std::string& extension_id,
    events::HistogramValue histogram_value,
    const char* event_name,
    const PermissionSet& changed_permissions) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (!event_router)
    return;

  scoped_ptr<base::ListValue> value(new base::ListValue());
  scoped_ptr<api::permissions::Permissions> permissions =
      PackPermissionSet(changed_permissions);
  value->Append(permissions->ToValue().release());
  scoped_ptr<Event> event(new Event(histogram_value, event_name, value.Pass()));
  event->restrict_to_browser_context = browser_context_;
  event_router->DispatchEventToExtension(extension_id, event.Pass());
}

void PermissionsUpdater::NotifyPermissionsUpdated(
    EventType event_type,
    const Extension* extension,
    const PermissionSet& changed) {
  DCHECK((init_flag_ & INIT_FLAG_TRANSIENT) == 0);
  if (changed.IsEmpty())
    return;

  UpdatedExtensionPermissionsInfo::Reason reason;
  events::HistogramValue histogram_value;
  const char* event_name = NULL;

  if (event_type == REMOVED) {
    reason = UpdatedExtensionPermissionsInfo::REMOVED;
    histogram_value = events::PERMISSIONS_ON_REMOVED;
    event_name = permissions::OnRemoved::kEventName;
  } else {
    CHECK_EQ(ADDED, event_type);
    reason = UpdatedExtensionPermissionsInfo::ADDED;
    histogram_value = events::PERMISSIONS_ON_ADDED;
    event_name = permissions::OnAdded::kEventName;
  }

  // Notify other APIs or interested parties.
  UpdatedExtensionPermissionsInfo info = UpdatedExtensionPermissionsInfo(
      extension, changed, reason);
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSION_PERMISSIONS_UPDATED,
      content::Source<Profile>(profile),
      content::Details<UpdatedExtensionPermissionsInfo>(&info));

  ExtensionMsg_UpdatePermissions_Params params;
  params.extension_id = extension->id();
  params.active_permissions = ExtensionMsg_PermissionSetStruct(
      extension->permissions_data()->active_permissions());
  params.withheld_permissions = ExtensionMsg_PermissionSetStruct(
      extension->permissions_data()->withheld_permissions());

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
  DispatchEvent(extension->id(), histogram_value, event_name, changed);
}

}  // namespace extensions

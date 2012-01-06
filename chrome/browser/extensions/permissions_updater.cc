// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/permissions_updater.h"

#include "base/json/json_writer.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_permissions_api_helpers.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/extension_permission_set.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"

using content::RenderProcessHost;
using extensions::permissions_api::PackPermissionsToValue;

namespace extensions {

namespace {

const char kOnAdded[] = "permissions.onAdded";
const char kOnRemoved[] = "permissions.onRemoved";

}

PermissionsUpdater::PermissionsUpdater(Profile* profile)
    : profile_(profile) {}

PermissionsUpdater::~PermissionsUpdater() {}

void PermissionsUpdater::AddPermissions(
    const Extension* extension, const ExtensionPermissionSet* permissions) {
  scoped_refptr<const ExtensionPermissionSet> existing(
      extension->GetActivePermissions());
  scoped_refptr<ExtensionPermissionSet> total(
      ExtensionPermissionSet::CreateUnion(existing, permissions));
  scoped_refptr<ExtensionPermissionSet> added(
      ExtensionPermissionSet::CreateDifference(total.get(), existing));

  UpdateActivePermissions(extension, total.get());

  // Update the granted permissions so we don't auto-disable the extension.
  GrantActivePermissions(extension);

  NotifyPermissionsUpdated(ADDED, extension, added.get());
}

void PermissionsUpdater::RemovePermissions(
    const Extension* extension, const ExtensionPermissionSet* permissions) {
  scoped_refptr<const ExtensionPermissionSet> existing(
      extension->GetActivePermissions());
  scoped_refptr<ExtensionPermissionSet> total(
      ExtensionPermissionSet::CreateDifference(existing, permissions));
  scoped_refptr<ExtensionPermissionSet> removed(
      ExtensionPermissionSet::CreateDifference(existing, total.get()));

  // We update the active permissions, and not the granted permissions, because
  // the extension, not the user, removed the permissions. This allows the
  // extension to add them again without prompting the user.
  UpdateActivePermissions(extension, total.get());

  NotifyPermissionsUpdated(REMOVED, extension, removed.get());
}

void PermissionsUpdater::GrantActivePermissions(const Extension* extension) {
  CHECK(extension);

  // We only maintain the granted permissions prefs for extensions that can't
  // silently increase their permissions.
  if (extension->CanSilentlyIncreasePermissions())
    return;

  GetExtensionPrefs()->AddGrantedPermissions(
      extension->id(), extension->GetActivePermissions());
}

void PermissionsUpdater::UpdateActivePermissions(
    const Extension* extension, const ExtensionPermissionSet* permissions) {
  GetExtensionPrefs()->SetActivePermissions(extension->id(), permissions);
  extension->SetActivePermissions(permissions);
}

void PermissionsUpdater::DispatchEvent(
    const std::string& extension_id,
    const char* event_name,
    const ExtensionPermissionSet* changed_permissions) {
  if (!profile_ || !profile_->GetExtensionEventRouter())
    return;

  ListValue value;
  value.Append(PackPermissionsToValue(changed_permissions));
  std::string json_value;
  base::JSONWriter::Write(&value, false, &json_value);
  profile_->GetExtensionEventRouter()->DispatchEventToExtension(
      extension_id, event_name, json_value, profile_, GURL());
}

void PermissionsUpdater::NotifyPermissionsUpdated(
    EventType event_type,
    const Extension* extension,
    const ExtensionPermissionSet* changed) {
  if (!changed || changed->IsEmpty())
    return;

  UpdatedExtensionPermissionsInfo::Reason reason;
  const char* event_name = NULL;

  if (event_type == REMOVED) {
    reason = UpdatedExtensionPermissionsInfo::REMOVED;
    event_name = kOnRemoved;
  } else {
    CHECK_EQ(ADDED, event_type);
    reason = UpdatedExtensionPermissionsInfo::ADDED;
    event_name = kOnAdded;
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
    if (profile_->IsSameProfile(profile))
      host->Send(new ExtensionMsg_UpdatePermissions(
          static_cast<int>(reason),
          extension->id(),
          changed->apis(),
          changed->explicit_hosts(),
          changed->scriptable_hosts()));
  }

  // Trigger the onAdded and onRemoved events in the extension.
  DispatchEvent(extension->id(), event_name, changed);
}

ExtensionPrefs* PermissionsUpdater::GetExtensionPrefs() {
  return profile_->GetExtensionService()->extension_prefs();
}

}  // namespace extensions

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/permissions_updater.h"

#include "base/json/json_writer.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/permissions/permissions_api_helpers.h"
#include "chrome/browser/extensions/extension_util.h"
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
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"

using content::RenderProcessHost;
using extensions::permissions_api_helpers::PackPermissionSet;

namespace extensions {

namespace permissions = api::permissions;

namespace {

// Returns a set of single origin permissions from |permissions| that match
// |bounds|. This is necessary for two reasons:
//   a) single origin active permissions can get filtered out in
//      GetBoundedActivePermissions because they are not recognized as a subset
//      of all-host permissions
//   b) active permissions that do not match any manifest permissions can
//      exist if a manifest permission is dropped
URLPatternSet FilterSingleOriginPermissions(const URLPatternSet& permissions,
                                            const URLPatternSet& bounds) {
  URLPatternSet single_origin_permissions;
  for (URLPatternSet::const_iterator iter = permissions.begin();
       iter != permissions.end();
       ++iter) {
    if (iter->MatchesSingleOrigin() &&
        bounds.MatchesURL(GURL(iter->GetAsString()))) {
      single_origin_permissions.AddPattern(*iter);
    }
  }
  return single_origin_permissions;
}

// Returns a PermissionSet that has the active permissions of the extension,
// bounded to its current manifest.
scoped_refptr<const PermissionSet> GetBoundedActivePermissions(
    const Extension* extension,
    const scoped_refptr<const PermissionSet>& active_permissions) {
  // If the extension has used the optional permissions API, it will have a
  // custom set of active permissions defined in the extension prefs. Here,
  // we update the extension's active permissions based on the prefs.
  if (!active_permissions)
    return extension->permissions_data()->active_permissions();

  scoped_refptr<const PermissionSet> required_permissions =
      PermissionsParser::GetRequiredPermissions(extension);

  // We restrict the active permissions to be within the bounds defined in the
  // extension's manifest.
  //  a) active permissions must be a subset of optional + default permissions
  //  b) active permissions must contains all default permissions
  scoped_refptr<PermissionSet> total_permissions = PermissionSet::CreateUnion(
      required_permissions,
      PermissionsParser::GetOptionalPermissions(extension));

  // Make sure the active permissions contain no more than optional + default.
  scoped_refptr<PermissionSet> adjusted_active =
      PermissionSet::CreateIntersection(total_permissions, active_permissions);

  // Make sure the active permissions contain the default permissions.
  adjusted_active =
      PermissionSet::CreateUnion(required_permissions, adjusted_active);

  return adjusted_active;
}

// Divvy up the |url patterns| between those we grant and those we do not. If
// |withhold_permissions| is false (because the requisite feature is not
// enabled), no permissions are withheld.
void SegregateUrlPermissions(const URLPatternSet& url_patterns,
                             bool withhold_permissions,
                             URLPatternSet* granted,
                             URLPatternSet* withheld) {
  for (URLPatternSet::const_iterator iter = url_patterns.begin();
       iter != url_patterns.end();
       ++iter) {
    if (withhold_permissions && iter->ImpliesAllHosts())
      withheld->AddPattern(*iter);
    else
      granted->AddPattern(*iter);
  }
}

}  // namespace

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

  SetPermissions(extension, total, NULL);

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
  SetPermissions(extension, total, NULL);

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

void PermissionsUpdater::InitializePermissions(const Extension* extension) {
  scoped_refptr<const PermissionSet> active_permissions =
      ExtensionPrefs::Get(browser_context_)
          ->GetActivePermissions(extension->id());
  scoped_refptr<const PermissionSet> bounded_active =
      GetBoundedActivePermissions(extension, active_permissions);

  // We withhold permissions iff the switch to do so is enabled, the extension
  // shows up in chrome:extensions (so the user can grant withheld permissions),
  // the extension is not part of chrome or corporate policy, and also not on
  // the scripting whitelist. Additionally, we don't withhold if the extension
  // has the preference to allow scripting on all urls.
  bool should_withhold_permissions =
      FeatureSwitch::scripts_require_action()->IsEnabled() &&
      extension->ShouldDisplayInExtensionSettings() &&
      !Manifest::IsPolicyLocation(extension->location()) &&
      !Manifest::IsComponentLocation(extension->location()) &&
      !PermissionsData::CanExecuteScriptEverywhere(extension) &&
      !util::AllowedScriptingOnAllUrls(extension->id(), browser_context_);

  URLPatternSet granted_explicit_hosts;
  URLPatternSet withheld_explicit_hosts;
  SegregateUrlPermissions(bounded_active->explicit_hosts(),
                          should_withhold_permissions,
                          &granted_explicit_hosts,
                          &withheld_explicit_hosts);

  URLPatternSet granted_scriptable_hosts;
  URLPatternSet withheld_scriptable_hosts;
  SegregateUrlPermissions(bounded_active->scriptable_hosts(),
                          should_withhold_permissions,
                          &granted_scriptable_hosts,
                          &withheld_scriptable_hosts);

  // After withholding permissions, add back any origins to the active set that
  // may have been lost during the set operations that would have dropped them.
  // For example, the union of <all_urls> and "example.com" is <all_urls>, so
  // we may lose "example.com". However, "example.com" is important once
  // <all_urls> is stripped during withholding.
  if (active_permissions) {
    granted_explicit_hosts.AddPatterns(
        FilterSingleOriginPermissions(active_permissions->explicit_hosts(),
                                      bounded_active->explicit_hosts()));
    granted_scriptable_hosts.AddPatterns(
        FilterSingleOriginPermissions(active_permissions->scriptable_hosts(),
                                      bounded_active->scriptable_hosts()));
  }

  bounded_active = new PermissionSet(bounded_active->apis(),
                                     bounded_active->manifest_permissions(),
                                     granted_explicit_hosts,
                                     granted_scriptable_hosts);

  scoped_refptr<const PermissionSet> withheld =
      new PermissionSet(APIPermissionSet(),
                        ManifestPermissionSet(),
                        withheld_explicit_hosts,
                        withheld_scriptable_hosts);
  SetPermissions(extension, bounded_active, withheld);
}

void PermissionsUpdater::WithholdImpliedAllHosts(const Extension* extension) {
  scoped_refptr<const PermissionSet> active =
      extension->permissions_data()->active_permissions();
  scoped_refptr<const PermissionSet> withheld =
      extension->permissions_data()->withheld_permissions();

  URLPatternSet withheld_scriptable = withheld->scriptable_hosts();
  URLPatternSet active_scriptable;
  SegregateUrlPermissions(active->scriptable_hosts(),
                          true,  // withhold permissions
                          &active_scriptable,
                          &withheld_scriptable);

  URLPatternSet withheld_explicit = withheld->explicit_hosts();
  URLPatternSet active_explicit;
  SegregateUrlPermissions(active->explicit_hosts(),
                          true,  // withhold permissions
                          &active_explicit,
                          &withheld_explicit);

  SetPermissions(extension,
                 new PermissionSet(active->apis(),
                                   active->manifest_permissions(),
                                   active_explicit,
                                   active_scriptable),
                  new PermissionSet(withheld->apis(),
                                    withheld->manifest_permissions(),
                                    withheld_explicit,
                                    withheld_scriptable));
  // TODO(rdevlin.cronin) We should notify the observers/renderer.
}

void PermissionsUpdater::GrantWithheldImpliedAllHosts(
    const Extension* extension) {
  scoped_refptr<const PermissionSet> active =
      extension->permissions_data()->active_permissions();
  scoped_refptr<const PermissionSet> withheld =
      extension->permissions_data()->withheld_permissions();

  // Move the all-hosts permission from withheld to active.
  // We can cheat a bit here since we know that the only host permission we
  // withhold is allhosts (or something similar enough to it), so we can just
  // grant all withheld host permissions.
  URLPatternSet explicit_hosts;
  URLPatternSet::CreateUnion(
      active->explicit_hosts(), withheld->explicit_hosts(), &explicit_hosts);
  URLPatternSet scriptable_hosts;
  URLPatternSet::CreateUnion(active->scriptable_hosts(),
                             withheld->scriptable_hosts(),
                             &scriptable_hosts);

  // Since we only withhold host permissions (so far), we know that withheld
  // permissions will be empty.
  SetPermissions(extension,
                 new PermissionSet(active->apis(),
                                   active->manifest_permissions(),
                                   explicit_hosts,
                                   scriptable_hosts),
                 new PermissionSet());
  // TODO(rdevlin.cronin) We should notify the observers/renderer.
}

void PermissionsUpdater::SetPermissions(
    const Extension* extension,
    const scoped_refptr<const PermissionSet>& active,
    scoped_refptr<const PermissionSet> withheld) {
  withheld = withheld.get() ? withheld
                 : extension->permissions_data()->withheld_permissions();
  extension->permissions_data()->SetPermissions(active, withheld);
  ExtensionPrefs::Get(browser_context_)->SetActivePermissions(
      extension->id(), active.get());
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
  DispatchEvent(extension->id(), event_name, changed);
}

}  // namespace extensions

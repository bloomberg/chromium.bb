// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/permissions_updater.h"

#include "base/json/json_writer.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/permissions/permissions_api_helpers.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/api/identity/oauth2_manifest_handler.h"
#include "chrome/common/extensions/api/permissions.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_messages.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "google_apis/gaia/oauth2_mint_token_flow.h"

using content::RenderProcessHost;
using extensions::permissions_api_helpers::PackPermissionSet;
using extensions::PermissionSet;

namespace extensions {

namespace {

const char kOnAdded[] = "permissions.onAdded";
const char kOnRemoved[] = "permissions.onRemoved";

// An object to link the lifetime of an OAuth2MintTokenFlow to a Profile.
// The flow should not outlive the profile because the request context will
// become invalid.
class OAuth2GrantRecorder : public OAuth2MintTokenFlow::Delegate,
                            public content::NotificationObserver {
 public:
  OAuth2GrantRecorder(Profile* profile, const Extension* extension)
    : ALLOW_THIS_IN_INITIALIZER_LIST(flow_(
          profile->GetRequestContext(),
          this,
          OAuth2MintTokenFlow::Parameters(
              TokenServiceFactory::GetForProfile(profile)->
                  GetOAuth2LoginRefreshToken(),
              extension->id(),
              OAuth2Info::GetOAuth2Info(extension).client_id,
              OAuth2Info::GetOAuth2Info(extension).scopes,
              OAuth2MintTokenFlow::MODE_RECORD_GRANT))) {
    notification_registrar_.Add(this,
                                chrome::NOTIFICATION_PROFILE_DESTROYED,
                                content::Source<Profile>(profile));

    flow_.Start();
  }

  // content::NotificationObserver:
  virtual void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) OVERRIDE {
    DCHECK_EQ(type, chrome::NOTIFICATION_PROFILE_DESTROYED);
    delete this;
  }

  // OAuth2MintTokenFlow::Delegate:
  virtual void OnMintTokenSuccess(const std::string& access_token) OVERRIDE {
    delete this;
  }
  virtual void OnIssueAdviceSuccess(
      const IssueAdviceInfo& issue_advice) OVERRIDE {
    delete this;
  }
  virtual void OnMintTokenFailure(
      const GoogleServiceAuthError& error) OVERRIDE {
    delete this;
  }

 private:
  virtual ~OAuth2GrantRecorder() {}

  OAuth2MintTokenFlow flow_;
  content::NotificationRegistrar notification_registrar_;
};

}  // namespace

PermissionsUpdater::PermissionsUpdater(Profile* profile)
    : profile_(profile) {}

PermissionsUpdater::~PermissionsUpdater() {}

void PermissionsUpdater::AddPermissions(
    const Extension* extension, const PermissionSet* permissions) {
  scoped_refptr<const PermissionSet> existing(
      extension->GetActivePermissions());
  scoped_refptr<PermissionSet> total(
      PermissionSet::CreateUnion(existing, permissions));
  scoped_refptr<PermissionSet> added(
      PermissionSet::CreateDifference(total.get(), existing));

  UpdateActivePermissions(extension, total.get());

  // Update the granted permissions so we don't auto-disable the extension.
  GrantActivePermissions(extension, false);

  NotifyPermissionsUpdated(ADDED, extension, added.get());
}

void PermissionsUpdater::RemovePermissions(
    const Extension* extension, const PermissionSet* permissions) {
  scoped_refptr<const PermissionSet> existing(
      extension->GetActivePermissions());
  scoped_refptr<PermissionSet> total(
      PermissionSet::CreateDifference(existing, permissions));
  scoped_refptr<PermissionSet> removed(
      PermissionSet::CreateDifference(existing, total.get()));

  // We update the active permissions, and not the granted permissions, because
  // the extension, not the user, removed the permissions. This allows the
  // extension to add them again without prompting the user.
  UpdateActivePermissions(extension, total.get());

  NotifyPermissionsUpdated(REMOVED, extension, removed.get());
}

void PermissionsUpdater::GrantActivePermissions(const Extension* extension,
                                                bool record_oauth2_grant) {
  CHECK(extension);

  // We only maintain the granted permissions prefs for INTERNAL and LOAD
  // extensions.
  if (!Manifest::IsUnpackedLocation(extension->location()) &&
      extension->location() != Manifest::INTERNAL)
    return;

  if (record_oauth2_grant) {
    // Only record OAuth grant if:
    // 1. The extension has client id and scopes.
    // 2. The user is signed in to Chrome.
    const OAuth2Info& oauth2_info = OAuth2Info::GetOAuth2Info(extension);
    if (!oauth2_info.client_id.empty() && !oauth2_info.scopes.empty()) {
      TokenService* token_service = TokenServiceFactory::GetForProfile(
          profile_);
      if (token_service && token_service->HasOAuthLoginToken()) {
        new OAuth2GrantRecorder(profile_, extension);
      }
    }
  }

  GetExtensionPrefs()->AddGrantedPermissions(extension->id(),
                                             extension->GetActivePermissions());
}

void PermissionsUpdater::UpdateActivePermissions(
    const Extension* extension, const PermissionSet* permissions) {
  GetExtensionPrefs()->SetActivePermissions(extension->id(), permissions);
  extension->SetActivePermissions(permissions);
}

void PermissionsUpdater::DispatchEvent(
    const std::string& extension_id,
    const char* event_name,
    const PermissionSet* changed_permissions) {
  if (!profile_ ||
      !ExtensionSystem::Get(profile_)->event_router())
    return;

  scoped_ptr<ListValue> value(new ListValue());
  scoped_ptr<api::permissions::Permissions> permissions =
      PackPermissionSet(changed_permissions);
  value->Append(permissions->ToValue().release());
  scoped_ptr<Event> event(new Event(event_name, value.Pass()));
  event->restrict_to_profile = profile_;
  ExtensionSystem::Get(profile_)->event_router()->
      DispatchEventToExtension(extension_id, event.Pass());
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
  return ExtensionSystem::Get(profile_)->extension_service()->extension_prefs();
}

}  // namespace extensions

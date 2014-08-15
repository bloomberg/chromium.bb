// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/active_tab_permission_granter.h"

#include "chrome/browser/extensions/active_script_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/user_script.h"
#include "url/gurl.h"

using content::RenderProcessHost;
using content::WebContentsObserver;

namespace extensions {

ActiveTabPermissionGranter::ActiveTabPermissionGranter(
    content::WebContents* web_contents,
    int tab_id,
    Profile* profile)
    : WebContentsObserver(web_contents),
      tab_id_(tab_id),
      extension_registry_observer_(this) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(profile));
}

ActiveTabPermissionGranter::~ActiveTabPermissionGranter() {}

void ActiveTabPermissionGranter::GrantIfRequested(const Extension* extension) {
  if (granted_extensions_.Contains(extension->id()))
    return;

  APIPermissionSet new_apis;
  URLPatternSet new_hosts;

  const PermissionsData* permissions_data = extension->permissions_data();

  // If the extension requested all-hosts but has had it withheld, we grant it
  // active tab-style permissions, even if it doesn't have the activeTab
  // permission in the manifest.
  if (permissions_data->HasAPIPermission(APIPermission::kActiveTab) ||
      permissions_data->HasWithheldImpliedAllHosts()) {
    new_hosts.AddOrigin(UserScript::ValidUserScriptSchemes(),
                        web_contents()->GetVisibleURL().GetOrigin());
    new_apis.insert(APIPermission::kTab);
  }

  if (permissions_data->HasAPIPermission(APIPermission::kTabCapture))
    new_apis.insert(APIPermission::kTabCaptureForTab);

  if (!new_apis.empty() || !new_hosts.is_empty()) {
    granted_extensions_.Insert(extension);
    scoped_refptr<const PermissionSet> new_permissions =
        new PermissionSet(new_apis, ManifestPermissionSet(),
                          new_hosts, URLPatternSet());
    permissions_data->UpdateTabSpecificPermissions(tab_id_, new_permissions);
    const content::NavigationEntry* navigation_entry =
        web_contents()->GetController().GetVisibleEntry();
    if (navigation_entry) {
      Send(new ExtensionMsg_UpdateTabSpecificPermissions(
          navigation_entry->GetURL(),
          tab_id_,
          extension->id(),
          new_hosts));
      // If more things ever need to know about this, we should consider making
      // an observer class.
      // It's important that this comes after the IPC is sent to the renderer,
      // so that any tasks executing in the renderer occur after it has the
      // updated permissions.
      ActiveScriptController::GetForWebContents(web_contents())
          ->OnActiveTabPermissionGranted(extension);
    }
  }
}

void ActiveTabPermissionGranter::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  if (details.is_in_page)
    return;
  DCHECK(details.is_main_frame);  // important: sub-frames don't get granted!
  ClearActiveExtensionsAndNotify();
}

void ActiveTabPermissionGranter::WebContentsDestroyed() {
  ClearActiveExtensionsAndNotify();
}

void ActiveTabPermissionGranter::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  // Note: don't need to clear the permissions (nor tell the renderer about it)
  // because it's being unloaded anyway.
  granted_extensions_.Remove(extension->id());
}

void ActiveTabPermissionGranter::ClearActiveExtensionsAndNotify() {
  if (granted_extensions_.is_empty())
    return;

  std::vector<std::string> extension_ids;

  for (ExtensionSet::const_iterator it = granted_extensions_.begin();
       it != granted_extensions_.end(); ++it) {
    it->get()->permissions_data()->ClearTabSpecificPermissions(tab_id_);
    extension_ids.push_back((*it)->id());
  }

  Send(new ExtensionMsg_ClearTabSpecificPermissions(tab_id_, extension_ids));
  granted_extensions_.Clear();
}

}  // namespace extensions

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/active_tab_permission_granter.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_id.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/user_script.h"

using content::RenderProcessHost;
using content::WebContentsObserver;

namespace extensions {

ActiveTabPermissionGranter::ActiveTabPermissionGranter(
    content::WebContents* web_contents, int tab_id, Profile* profile)
    : WebContentsObserver(web_contents), tab_id_(tab_id) {
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED,
                 content::Source<Profile>(profile));
}

ActiveTabPermissionGranter::~ActiveTabPermissionGranter() {}

void ActiveTabPermissionGranter::GrantIfRequested(const Extension* extension) {
  // Active tab grant request implies there was a user gesture.
  web_contents()->UserGestureDone();

  if (granted_extensions_.Contains(extension->id()))
    return;

  APIPermissionSet new_apis;
  URLPatternSet new_hosts;

  if (extension->HasAPIPermission(APIPermission::kActiveTab)) {
    URLPattern pattern(UserScript::ValidUserScriptSchemes());
    // Pattern parsing could fail if this is an unsupported URL e.g. chrome://.
    if (pattern.Parse(web_contents()->GetURL().spec()) ==
            URLPattern::PARSE_SUCCESS) {
      new_hosts.AddPattern(pattern);
    }
    new_apis.insert(APIPermission::kTab);
  }

  if (extension->HasAPIPermission(APIPermission::kTabCapture))
    new_apis.insert(APIPermission::kTabCaptureForTab);

  if (!new_apis.empty() || !new_hosts.is_empty()) {
    granted_extensions_.Insert(extension);
    scoped_refptr<const PermissionSet> new_permissions =
        new PermissionSet(new_apis, ManifestPermissionSet(),
                          new_hosts, URLPatternSet());
    PermissionsData::UpdateTabSpecificPermissions(extension,
                                                  tab_id_,
                                                  new_permissions);
    const content::NavigationEntry* navigation_entry =
        web_contents()->GetController().GetVisibleEntry();
    if (navigation_entry) {
      Send(new ExtensionMsg_UpdateTabSpecificPermissions(
          navigation_entry->GetPageID(),
          tab_id_,
          extension->id(),
          new_hosts));
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

void ActiveTabPermissionGranter::WebContentsDestroyed(
    content::WebContents* web_contents) {
  ClearActiveExtensionsAndNotify();
}

void ActiveTabPermissionGranter::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED);
  const Extension* extension =
      content::Details<UnloadedExtensionInfo>(details)->extension;
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
    PermissionsData::ClearTabSpecificPermissions(it->get(), tab_id_);
    extension_ids.push_back((*it)->id());
  }

  Send(new ExtensionMsg_ClearTabSpecificPermissions(tab_id_, extension_ids));
  granted_extensions_.Clear();
}

}  // namespace extensions

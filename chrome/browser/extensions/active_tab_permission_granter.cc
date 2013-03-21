// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/active_tab_permission_granter.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/permissions/permission_set.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

using content::RenderProcessHost;
using content::WebContentsObserver;

namespace extensions {

ActiveTabPermissionGranter::ActiveTabPermissionGranter(
    content::WebContents* web_contents, int tab_id, Profile* profile)
    : WebContentsObserver(web_contents), tab_id_(tab_id) {
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile));
}

ActiveTabPermissionGranter::~ActiveTabPermissionGranter() {}

void ActiveTabPermissionGranter::GrantIfRequested(const Extension* extension) {
  if (!extension->HasAPIPermission(extensions::APIPermission::kActiveTab))
    return;

  if (IsGranted(extension))
    return;

  URLPattern pattern(UserScript::ValidUserScriptSchemes());
  if (pattern.Parse(web_contents()->GetURL().spec()) !=
          URLPattern::PARSE_SUCCESS) {
    // Pattern parsing could fail if this is an unsupported URL e.g. chrome://.
    return;
  }

  APIPermissionSet new_apis;
  new_apis.insert(APIPermission::kTab);
  URLPatternSet new_hosts;
  new_hosts.AddPattern(pattern);
  scoped_refptr<const PermissionSet> new_permissions =
      new PermissionSet(new_apis, new_hosts, URLPatternSet());

  extension->UpdateTabSpecificPermissions(tab_id_, new_permissions);
  granted_extensions_.Insert(extension);
  Send(new ExtensionMsg_UpdateTabSpecificPermissions(GetPageID(),
                                                     tab_id_,
                                                     extension->id(),
                                                     new_hosts));
}

bool ActiveTabPermissionGranter::IsGranted(const Extension* extension) {
  return granted_extensions_.Contains(extension->id());
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
  DCHECK_EQ(type, chrome::NOTIFICATION_EXTENSION_UNLOADED);
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
    (*it)->ClearTabSpecificPermissions(tab_id_);
    extension_ids.push_back((*it)->id());
  }

  Send(new ExtensionMsg_ClearTabSpecificPermissions(tab_id_, extension_ids));
  granted_extensions_.Clear();
}

int32 ActiveTabPermissionGranter::GetPageID() {
  return web_contents()->GetController().GetActiveEntry()->GetPageID();
}

}  // namespace extensions

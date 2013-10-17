// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/active_tab_permission_granter.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/permissions/permission_set.h"
#include "chrome/common/extensions/permissions/permissions_data.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/user_script.h"

namespace extensions {

ActiveTabPermissionGranter::ActiveTabPermissionGranter(
    content::WebContents* web_contents,
    int tab_id,
    Profile* profile)
    : tab_id_(tab_id),
      web_contents_(web_contents),
      tab_capability_tracker_(web_contents, profile) {
  tab_capability_tracker_.AddObserver(this);
}

ActiveTabPermissionGranter::~ActiveTabPermissionGranter() {
  tab_capability_tracker_.RemoveObserver(this);
}

void ActiveTabPermissionGranter::GrantIfRequested(const Extension* extension) {
  tab_capability_tracker_.Grant(extension);
}

void ActiveTabPermissionGranter::OnGranted(const Extension* extension) {
  APIPermissionSet new_apis;
  URLPatternSet new_hosts;

  if (extension->HasAPIPermission(APIPermission::kActiveTab)) {
    URLPattern pattern(UserScript::ValidUserScriptSchemes());
    // Pattern parsing could fail if this is an unsupported URL e.g. chrome://.
    if (pattern.Parse(web_contents_->GetURL().spec()) ==
            URLPattern::PARSE_SUCCESS) {
      new_hosts.AddPattern(pattern);
    }
    new_apis.insert(APIPermission::kTab);
  }

  if (extension->HasAPIPermission(APIPermission::kTabCapture))
    new_apis.insert(APIPermission::kTabCaptureForTab);

  if (!new_apis.empty() || !new_hosts.is_empty()) {
    scoped_refptr<const PermissionSet> new_permissions =
        new PermissionSet(new_apis, new_hosts, URLPatternSet());
    PermissionsData::UpdateTabSpecificPermissions(extension,
                                                  tab_id_,
                                                  new_permissions);
    web_contents_->Send(
        new ExtensionMsg_UpdateTabSpecificPermissions(GetPageID(),
                                                      tab_id_,
                                                      extension->id(),
                                                      new_hosts));
  }
}

void ActiveTabPermissionGranter::OnRevoked(const ExtensionSet* extensions) {
  std::vector<std::string> extension_ids;

  for (ExtensionSet::const_iterator it = extensions->begin();
       it != extensions->end(); ++it) {
    PermissionsData::ClearTabSpecificPermissions(it->get(), tab_id_);
    extension_ids.push_back((*it)->id());
  }

  web_contents_->Send(
      new ExtensionMsg_ClearTabSpecificPermissions(tab_id_, extension_ids));
}

int32 ActiveTabPermissionGranter::GetPageID() {
  return web_contents_->GetController().GetVisibleEntry()->GetPageID();
}

}  // namespace extensions

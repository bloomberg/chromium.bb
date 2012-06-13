// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/active_tab_permission_manager.h"

#include "chrome/browser/extensions/extension_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/extension_permission_set.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

using content::RenderProcessHost;
using content::WebContentsObserver;

namespace extensions {

ActiveTabPermissionManager::ActiveTabPermissionManager(
    TabContents* tab_contents)
    : WebContentsObserver(tab_contents->web_contents()),
      tab_contents_(tab_contents) {
  InsertActiveURL(web_contents()->GetURL());
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(tab_contents->profile()));
}

ActiveTabPermissionManager::~ActiveTabPermissionManager() {}

void ActiveTabPermissionManager::GrantIfRequested(const Extension* extension) {
  if (!extension->HasAPIPermission(ExtensionAPIPermission::kActiveTab))
    return;

  if (active_urls_.is_empty())
    return;

  // Only need to check the number of permissions here rather than the URLs
  // themselves, because the set can only ever grow.
  const URLPatternSet* old_permissions =
      extension->GetTabSpecificHostPermissions(tab_id());
  if (old_permissions && (old_permissions->size() == active_urls_.size()))
    return;

  granted_.Insert(extension);
  extension->SetTabSpecificHostPermissions(tab_id(), active_urls_);
  Send(new ExtensionMsg_UpdateTabSpecificPermissions(GetPageID(),
                                                     tab_id(),
                                                     extension->id(),
                                                     active_urls_));
}

void ActiveTabPermissionManager::DidCommitProvisionalLoadForFrame(
    int64 frame_id,
    bool is_main_frame,
    const GURL& url,
    content::PageTransition transition_type,
    content::RenderViewHost* render_view_host) {
  if (is_main_frame)
    ClearActiveURLsAndNotify();
  InsertActiveURL(url);
}

void ActiveTabPermissionManager::WebContentsDestroyed(
    content::WebContents* web_contents) {
  ClearActiveURLsAndNotify();
}

void ActiveTabPermissionManager::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_EXTENSION_UNLOADED);
  const Extension* extension =
      content::Details<UnloadedExtensionInfo>(details)->extension;
  extension->ClearTabSpecificHostPermissions(tab_id());
  std::vector<std::string> single_id(1, extension->id());
  Send(new ExtensionMsg_ClearTabSpecificPermissions(tab_id(), single_id));
  granted_.Remove(extension->id());
}

void ActiveTabPermissionManager::ClearActiveURLsAndNotify() {
  active_urls_.ClearPatterns();

  if (granted_.is_empty())
    return;

  std::vector<std::string> extension_ids;

  for (ExtensionSet::const_iterator it = granted_.begin();
       it != granted_.end(); ++it) {
    (*it)->ClearTabSpecificHostPermissions(tab_id());
    extension_ids.push_back((*it)->id());
  }

  Send(new ExtensionMsg_ClearTabSpecificPermissions(tab_id(), extension_ids));
  granted_.Clear();
}

void ActiveTabPermissionManager::InsertActiveURL(const GURL& url) {
  URLPattern pattern(UserScript::kValidUserScriptSchemes);
  if (pattern.Parse(url.spec()) == URLPattern::PARSE_SUCCESS)
    active_urls_.AddPattern(pattern);
}

int32 ActiveTabPermissionManager::tab_id() {
  return tab_contents_->extension_tab_helper()->tab_id();
}

int32 ActiveTabPermissionManager::GetPageID() {
  return tab_contents_->web_contents()->GetController().GetActiveEntry()->
      GetPageID();
}

}  // namespace extensions

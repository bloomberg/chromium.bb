// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/tab_contents_synced_tab_delegate.h"

#include "base/memory/ref_counted.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/common/features.h"
#include "components/sessions/content/content_serialized_navigation_builder.h"
#include "components/sync_sessions/sync_sessions_client.h"
#include "components/sync_sessions/synced_window_delegate.h"
#include "components/sync_sessions/synced_window_delegates_getter.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "extensions/features/features.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/tab_helper.h"
#include "extensions/common/extension.h"
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"
#endif

using content::NavigationEntry;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(TabContentsSyncedTabDelegate);

namespace {

// Helper to access the correct NavigationEntry, accounting for pending entries.
NavigationEntry* GetPossiblyPendingEntryAtIndex(
    content::WebContents* web_contents,
    int i) {
  int pending_index = web_contents->GetController().GetPendingEntryIndex();
  return (pending_index == i)
             ? web_contents->GetController().GetPendingEntry()
             : web_contents->GetController().GetEntryAtIndex(i);
}

}  // namespace

TabContentsSyncedTabDelegate::TabContentsSyncedTabDelegate(
    content::WebContents* web_contents)
    : web_contents_(web_contents), sync_session_id_(0) {}

TabContentsSyncedTabDelegate::~TabContentsSyncedTabDelegate() {}

SessionID::id_type TabContentsSyncedTabDelegate::GetWindowId() const {
  return SessionTabHelper::FromWebContents(web_contents_)->window_id().id();
}

SessionID::id_type TabContentsSyncedTabDelegate::GetSessionId() const {
  return SessionTabHelper::FromWebContents(web_contents_)->session_id().id();
}

bool TabContentsSyncedTabDelegate::IsBeingDestroyed() const {
  return web_contents_->IsBeingDestroyed();
}

std::string TabContentsSyncedTabDelegate::GetExtensionAppId() const {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  const scoped_refptr<const extensions::Extension> extension_app(
      extensions::TabHelper::FromWebContents(web_contents_)->extension_app());
  if (extension_app.get())
    return extension_app->id();
#endif
  return std::string();
}

bool TabContentsSyncedTabDelegate::IsInitialBlankNavigation() const {
  return web_contents_->GetController().IsInitialBlankNavigation();
}

int TabContentsSyncedTabDelegate::GetCurrentEntryIndex() const {
  return web_contents_->GetController().GetCurrentEntryIndex();
}

int TabContentsSyncedTabDelegate::GetEntryCount() const {
  return web_contents_->GetController().GetEntryCount();
}

GURL TabContentsSyncedTabDelegate::GetVirtualURLAtIndex(int i) const {
  NavigationEntry* entry = GetPossiblyPendingEntryAtIndex(web_contents_, i);
  return entry ? entry->GetVirtualURL() : GURL();
}

GURL TabContentsSyncedTabDelegate::GetFaviconURLAtIndex(int i) const {
  NavigationEntry* entry = GetPossiblyPendingEntryAtIndex(web_contents_, i);
  return (entry->GetFavicon().valid ? entry->GetFavicon().url : GURL());
}

ui::PageTransition TabContentsSyncedTabDelegate::GetTransitionAtIndex(
    int i) const {
  NavigationEntry* entry = GetPossiblyPendingEntryAtIndex(web_contents_, i);
  return entry->GetTransitionType();
}

void TabContentsSyncedTabDelegate::GetSerializedNavigationAtIndex(
    int i,
    sessions::SerializedNavigationEntry* serialized_entry) const {
  NavigationEntry* entry = GetPossiblyPendingEntryAtIndex(web_contents_, i);
  *serialized_entry =
      sessions::ContentSerializedNavigationBuilder::FromNavigationEntry(i,
                                                                        *entry);
}

bool TabContentsSyncedTabDelegate::ProfileIsSupervised() const {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext())
      ->IsSupervised();
}

const std::vector<std::unique_ptr<const sessions::SerializedNavigationEntry>>*
TabContentsSyncedTabDelegate::GetBlockedNavigations() const {
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  SupervisedUserNavigationObserver* navigation_observer =
      SupervisedUserNavigationObserver::FromWebContents(web_contents_);
  DCHECK(navigation_observer);
  return &navigation_observer->blocked_navigations();
#else
  NOTREACHED();
  return nullptr;
#endif
}

bool TabContentsSyncedTabDelegate::IsPlaceholderTab() const {
  return false;
}

int TabContentsSyncedTabDelegate::GetSyncId() const {
  return sync_session_id_;
}

void TabContentsSyncedTabDelegate::SetSyncId(int sync_id) {
  sync_session_id_ = sync_id;
}

bool TabContentsSyncedTabDelegate::ShouldSync(
    sync_sessions::SyncSessionsClient* sessions_client) {
  if (sessions_client->GetSyncedWindowDelegatesGetter()->FindById(
          GetWindowId()) == nullptr)
    return false;

  // Is there a valid NavigationEntry?
  if (ProfileIsSupervised() && GetBlockedNavigations()->size() > 0)
    return true;

  if (IsInitialBlankNavigation())
    return false;  // This deliberately ignores a new pending entry.

  int entry_count = GetEntryCount();
  for (int i = 0; i < entry_count; ++i) {
    const GURL& virtual_url = GetVirtualURLAtIndex(i);
    if (!virtual_url.is_valid())
      continue;

    if (sessions_client->ShouldSyncURL(virtual_url))
      return true;
  }
  return false;
}

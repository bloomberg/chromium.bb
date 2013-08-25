// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/tab_contents_synced_tab_delegate.h"

#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/sync/glue/synced_window_delegate.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

#if defined(ENABLE_MANAGED_USERS)
#include "chrome/browser/managed_mode/managed_mode_navigation_observer.h"
#endif

using content::NavigationEntry;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(TabContentsSyncedTabDelegate);

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

Profile* TabContentsSyncedTabDelegate::profile() const {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}

std::string TabContentsSyncedTabDelegate::GetExtensionAppId() const {
  const scoped_refptr<const extensions::Extension> extension_app(
      extensions::TabHelper::FromWebContents(web_contents_)->extension_app());
  return (extension_app.get() ? extension_app->id() : std::string());
}

int TabContentsSyncedTabDelegate::GetCurrentEntryIndex() const {
  return web_contents_->GetController().GetCurrentEntryIndex();
}

int TabContentsSyncedTabDelegate::GetEntryCount() const {
  return web_contents_->GetController().GetEntryCount();
}

int TabContentsSyncedTabDelegate::GetPendingEntryIndex() const {
  return web_contents_->GetController().GetPendingEntryIndex();
}

NavigationEntry* TabContentsSyncedTabDelegate::GetPendingEntry() const {
  return web_contents_->GetController().GetPendingEntry();
}

NavigationEntry* TabContentsSyncedTabDelegate::GetEntryAtIndex(int i) const {
  return web_contents_->GetController().GetEntryAtIndex(i);
}

NavigationEntry* TabContentsSyncedTabDelegate::GetActiveEntry() const {
  return web_contents_->GetController().GetActiveEntry();
}

bool TabContentsSyncedTabDelegate::ProfileIsManaged() const {
  return profile()->IsManaged();
}

const std::vector<const content::NavigationEntry*>*
TabContentsSyncedTabDelegate::GetBlockedNavigations() const {
#if defined(ENABLE_MANAGED_USERS)
  ManagedModeNavigationObserver* navigation_observer =
      ManagedModeNavigationObserver::FromWebContents(web_contents_);
  DCHECK(navigation_observer);
  return navigation_observer->blocked_navigations();
#else
  NOTREACHED();
  return NULL;
#endif
}

bool TabContentsSyncedTabDelegate::IsPinned() const {
  const browser_sync::SyncedWindowDelegate* window =
      browser_sync::SyncedWindowDelegate::FindSyncedWindowDelegateWithId(
          GetWindowId());
  // We might not have a parent window, e.g. Developer Tools.
  return window ? window->IsTabPinned(this) : false;
}

bool TabContentsSyncedTabDelegate::HasWebContents() const { return true; }

content::WebContents* TabContentsSyncedTabDelegate::GetWebContents() const {
  return web_contents_;
}

int TabContentsSyncedTabDelegate::GetSyncId() const {
  return sync_session_id_;
}

void TabContentsSyncedTabDelegate::SetSyncId(int sync_id) {
  sync_session_id_ = sync_id;
}

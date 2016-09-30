// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/sessions/tab_delegate_sync_adapter.h"

#include "components/sync/driver/sync_service.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"

using syncer::SyncService;
using sync_sessions::OpenTabsUIDelegate;

namespace ntp_snippets {

TabDelegateSyncAdapter::TabDelegateSyncAdapter(SyncService* sync_service)
    : sync_service_(sync_service) {
  sync_service_->AddObserver(this);
}

TabDelegateSyncAdapter::~TabDelegateSyncAdapter() {
  sync_service_->RemoveObserver(this);
}

bool TabDelegateSyncAdapter::HasSessionsData() {
  // GetOpenTabsUIDelegate will be a nullptr if sync has not started, or if the
  // sessions data type is not enabled.
  return sync_service_->GetOpenTabsUIDelegate() != nullptr;
}

std::vector<const sync_sessions::SyncedSession*>
TabDelegateSyncAdapter::GetAllForeignSessions() {
  std::vector<const sync_sessions::SyncedSession*> sessions;
  OpenTabsUIDelegate* delegate = sync_service_->GetOpenTabsUIDelegate();
  if (delegate != nullptr) {
    // The return bool from GetAllForeignSessions(...) is ignored.
    delegate->GetAllForeignSessions(&sessions);
  }
  return sessions;
}

void TabDelegateSyncAdapter::SubscribeForForeignTabChange(
    const base::Closure& change_callback) {
  DCHECK(change_callback_.is_null());
  change_callback_ = change_callback;
}

void TabDelegateSyncAdapter::OnStateChanged() {
  // Ignored.
}

void TabDelegateSyncAdapter::OnSyncConfigurationCompleted() {
  InvokeCallback();
}

void TabDelegateSyncAdapter::OnForeignSessionUpdated() {
  InvokeCallback();
}

void TabDelegateSyncAdapter::InvokeCallback() {
  if (!change_callback_.is_null())
    change_callback_.Run();
}

}  // namespace ntp_snippets

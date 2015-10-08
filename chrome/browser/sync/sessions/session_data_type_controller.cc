// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sessions/session_data_type_controller.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/sync_driver/glue/synced_window_delegate.h"
#include "components/sync_driver/sessions/synced_window_delegates_getter.h"
#include "components/sync_driver/sync_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"

using content::BrowserThread;

namespace browser_sync {

SessionDataTypeController::SessionDataTypeController(
    const base::Closure& error_callback,
    sync_driver::SyncClient* sync_client,
    Profile* profile,
    SyncedWindowDelegatesGetter* synced_window_getter,
    sync_driver::LocalDeviceInfoProvider* local_device)
    : UIDataTypeController(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
          error_callback,
          syncer::SESSIONS,
          sync_client),
      sync_client_(sync_client),
      profile_(profile),
      synced_window_getter_(synced_window_getter),
      local_device_(local_device),
      waiting_on_session_restore_(false),
      waiting_on_local_device_info_(false) {
  DCHECK(local_device_);
  pref_registrar_.Init(sync_client_->GetPrefService());
  pref_registrar_.Add(
      prefs::kSavingBrowserHistoryDisabled,
      base::Bind(&SessionDataTypeController::OnSavingBrowserHistoryPrefChanged,
                 base::Unretained(this)));
}

SessionDataTypeController::~SessionDataTypeController() {}

bool SessionDataTypeController::StartModels() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::set<const browser_sync::SyncedWindowDelegate*> window =
      synced_window_getter_->GetSyncedWindowDelegates();
  for (std::set<const browser_sync::SyncedWindowDelegate*>::const_iterator i =
      window.begin(); i != window.end(); ++i) {
    if ((*i)->IsSessionRestoreInProgress()) {
      notification_registrar_.Add(
          this,
          chrome::NOTIFICATION_SESSION_RESTORE_COMPLETE,
          content::Source<Profile>(profile_));
      waiting_on_session_restore_ = true;
      break;
    }
  }

  if (!local_device_->GetLocalDeviceInfo()) {
    subscription_ = local_device_->RegisterOnInitializedCallback(
        base::Bind(&SessionDataTypeController::OnLocalDeviceInfoInitialized,
                   this));
    waiting_on_local_device_info_ = true;
  }

  return !IsWaiting();
}

void SessionDataTypeController::StopModels() {
  subscription_.reset();
  notification_registrar_.RemoveAll();
}

bool SessionDataTypeController::ReadyForStart() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return !sync_client_->GetPrefService()->GetBoolean(
      prefs::kSavingBrowserHistoryDisabled);
}

bool SessionDataTypeController::IsWaiting() {
  return waiting_on_session_restore_ || waiting_on_local_device_info_;
}

void SessionDataTypeController::MaybeCompleteLoading() {
  if (state_ == MODEL_STARTING && !IsWaiting()) {
    OnModelLoaded();
  }
}

void SessionDataTypeController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(chrome::NOTIFICATION_SESSION_RESTORE_COMPLETE, type);
  DCHECK_EQ(profile_, content::Source<Profile>(source).ptr());
  notification_registrar_.RemoveAll();

  waiting_on_session_restore_ = false;
  MaybeCompleteLoading();
}

void SessionDataTypeController::OnLocalDeviceInfoInitialized() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  subscription_.reset();

  waiting_on_local_device_info_ = false;
  MaybeCompleteLoading();
}

void SessionDataTypeController::OnSavingBrowserHistoryPrefChanged() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (sync_client_->GetPrefService()->GetBoolean(
          prefs::kSavingBrowserHistoryDisabled)) {
    // If history and tabs persistence is turned off then generate an
    // unrecoverable error. SESSIONS won't be a registered type on the next
    // Chrome restart.
    if (state() != NOT_RUNNING && state() != STOPPING) {
      syncer::SyncError error(
          FROM_HERE,
          syncer::SyncError::DATATYPE_POLICY_ERROR,
          "History and tab saving is now disabled by policy.",
          syncer::SESSIONS);
      OnSingleDataTypeUnrecoverableError(error);
    }
  }
}

}  // namespace browser_sync

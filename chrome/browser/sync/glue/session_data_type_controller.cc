// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/session_data_type_controller.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/sync/glue/synced_window_delegate.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"

namespace browser_sync {

SessionDataTypeController::SessionDataTypeController(
    ProfileSyncComponentsFactory* profile_sync_factory,
    Profile* profile,
    ProfileSyncService* sync_service)
    : FrontendDataTypeController(profile_sync_factory,
                                 profile,
                                 sync_service) {
}

SessionModelAssociator* SessionDataTypeController::GetModelAssociator() {
  return reinterpret_cast<SessionModelAssociator*>(model_associator_.get());
}

syncable::ModelType SessionDataTypeController::type() const {
  return syncable::SESSIONS;
}

bool SessionDataTypeController::StartModels() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  std::set<browser_sync::SyncedWindowDelegate*> window =
      browser_sync::SyncedWindowDelegate::GetSyncedWindowDelegates();
  for (std::set<browser_sync::SyncedWindowDelegate*>::const_iterator i =
      window.begin(); i != window.end(); ++i) {
    if ((*i)->IsSessionRestoreInProgress()) {
      notification_registrar_.Add(
          this,
          chrome::NOTIFICATION_SESSION_RESTORE_COMPLETE,
          content::Source<Profile>(profile_));
      return false;
    }
  }
  return true;
}

// Cleanup for our extra registrar usage.
void SessionDataTypeController::CleanUpState() {
  notification_registrar_.RemoveAll();
}

void SessionDataTypeController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK_EQ(chrome::NOTIFICATION_SESSION_RESTORE_COMPLETE, type);
  DCHECK_EQ(profile_, content::Source<Profile>(source).ptr());
  notification_registrar_.RemoveAll();
  OnModelLoaded();
}

SessionDataTypeController::~SessionDataTypeController() {}

void SessionDataTypeController::CreateSyncComponents() {
  ProfileSyncComponentsFactory::SyncComponents sync_components =
      profile_sync_factory_->CreateSessionSyncComponents(sync_service_, this);
  set_model_associator(sync_components.model_associator);
  set_change_processor(sync_components.change_processor);
}

}  // namespace browser_sync

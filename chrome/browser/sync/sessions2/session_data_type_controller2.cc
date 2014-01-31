// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sessions2/session_data_type_controller2.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/sync/glue/chrome_report_unrecoverable_error.h"
#include "chrome/browser/sync/glue/synced_window_delegate.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"

using content::BrowserThread;

namespace browser_sync {

SessionDataTypeController2::SessionDataTypeController2(
    ProfileSyncComponentsFactory* profile_sync_factory,
    Profile* profile,
    ProfileSyncService* sync_service)
    : UIDataTypeController(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
          base::Bind(&ChromeReportUnrecoverableError),
          syncer::SESSIONS,
          profile_sync_factory,
          profile,
          sync_service) {
}

SessionDataTypeController2::~SessionDataTypeController2() {}

bool SessionDataTypeController2::StartModels() {
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

void SessionDataTypeController2::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK_EQ(chrome::NOTIFICATION_SESSION_RESTORE_COMPLETE, type);
  DCHECK_EQ(profile_, content::Source<Profile>(source).ptr());
  notification_registrar_.RemoveAll();
  OnModelLoaded();
}

}  // namespace browser_sync

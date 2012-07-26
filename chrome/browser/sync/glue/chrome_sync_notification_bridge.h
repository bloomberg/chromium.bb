// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_CHROME_SYNC_NOTIFICATION_BRIDGE_H_
#define CHROME_BROWSER_SYNC_GLUE_CHROME_SYNC_NOTIFICATION_BRIDGE_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "sync/internal_api/public/base/model_type.h"

class Profile;

namespace syncer {
class SyncNotifierObserver;
}  // namespace

namespace browser_sync {

// A bridge that converts Chrome events on the UI thread to sync
// notifications on the sync task runner.
//
// Listens to NOTIFICATION_SYNC_REFRESH_LOCAL and
// NOTIFICATION_SYNC_REFRESH_REMOTE (on the UI thread) and triggers
// each observer's OnIncomingNotification method on these
// notifications (on the sync task runner).
class ChromeSyncNotificationBridge : public content::NotificationObserver {
 public:
  // Must be created and destroyed on the UI thread.
  ChromeSyncNotificationBridge(
      const Profile* profile,
      const scoped_refptr<base::SequencedTaskRunner>& sync_task_runner);
  virtual ~ChromeSyncNotificationBridge();

  // Must be called on the sync task runner.
  void UpdateEnabledTypes(syncer::ModelTypeSet enabled_types);
  // Marked virtual for tests.
  virtual void AddObserver(syncer::SyncNotifierObserver* observer);
  virtual void RemoveObserver(syncer::SyncNotifierObserver* observer);

  // NotificationObserver implementation. Called on UI thread.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  // Inner class to hold all the bits used on |sync_task_runner_|.
  class Core;

  const scoped_refptr<base::SequencedTaskRunner> sync_task_runner_;

  // Created on the UI thread, used only on |sync_task_runner_|.
  const scoped_refptr<Core> core_;

  // Used only on the UI thread.
  content::NotificationRegistrar registrar_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_CHROME_SYNC_NOTIFICATION_BRIDGE_H_

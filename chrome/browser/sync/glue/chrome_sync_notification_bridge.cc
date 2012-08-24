// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/chrome_sync_notification_bridge.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "sync/notifier/sync_notifier_observer.h"
#include "sync/notifier/sync_notifier_registrar.h"

using content::BrowserThread;

namespace browser_sync {

class ChromeSyncNotificationBridge::Core
    : public base::RefCountedThreadSafe<Core> {
 public:
  // Created on UI thread.
  explicit Core(
      const scoped_refptr<base::SequencedTaskRunner>& sync_task_runner);

  // All member functions below must be called on the sync task runner.

  void InitializeOnSyncThread();
  void CleanupOnSyncThread();

  void UpdateEnabledTypes(syncer::ModelTypeSet enabled_types);
  void RegisterHandler(syncer::SyncNotifierObserver* handler);
  void UpdateRegisteredIds(syncer::SyncNotifierObserver* handler,
                           const syncer::ObjectIdSet& ids);
  void UnregisterHandler(syncer::SyncNotifierObserver* handler);

  void EmitNotification(
      const syncer::ModelTypeStateMap& state_map,
      syncer::IncomingNotificationSource notification_source);

 private:
  friend class base::RefCountedThreadSafe<Core>;

  // Destroyed on the UI thread or on |sync_task_runner_|.
  ~Core();

  const scoped_refptr<base::SequencedTaskRunner> sync_task_runner_;

  // Used only on |sync_task_runner_|.
  syncer::ModelTypeSet enabled_types_;
  scoped_ptr<syncer::SyncNotifierRegistrar> notifier_registrar_;
};

ChromeSyncNotificationBridge::Core::Core(
    const scoped_refptr<base::SequencedTaskRunner>& sync_task_runner)
    : sync_task_runner_(sync_task_runner) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(sync_task_runner_.get());
}

ChromeSyncNotificationBridge::Core::~Core() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         sync_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(!notifier_registrar_.get());
}

void ChromeSyncNotificationBridge::Core::InitializeOnSyncThread() {
  notifier_registrar_.reset(new syncer::SyncNotifierRegistrar());
}

void ChromeSyncNotificationBridge::Core::CleanupOnSyncThread() {
  notifier_registrar_.reset();
}

void ChromeSyncNotificationBridge::Core::UpdateEnabledTypes(
    syncer::ModelTypeSet types) {
  DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
  enabled_types_ = types;
}

void ChromeSyncNotificationBridge::Core::RegisterHandler(
    syncer::SyncNotifierObserver* handler) {
  DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
  notifier_registrar_->RegisterHandler(handler);
}

void ChromeSyncNotificationBridge::Core::UpdateRegisteredIds(
    syncer::SyncNotifierObserver* handler,
    const syncer::ObjectIdSet& ids) {
  DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
  notifier_registrar_->UpdateRegisteredIds(handler, ids);
}

void ChromeSyncNotificationBridge::Core::UnregisterHandler(
    syncer::SyncNotifierObserver* handler) {
  DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
  notifier_registrar_->UnregisterHandler(handler);
}

void ChromeSyncNotificationBridge::Core::EmitNotification(
    const syncer::ModelTypeStateMap& state_map,
    syncer::IncomingNotificationSource notification_source) {
  DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
  const syncer::ModelTypeStateMap& effective_state_map =
      state_map.empty() ?
      syncer::ModelTypeSetToStateMap(enabled_types_, std::string()) :
      state_map;

  notifier_registrar_->DispatchInvalidationsToHandlers(
      ModelTypeStateMapToObjectIdStateMap(effective_state_map),
      notification_source);
}

ChromeSyncNotificationBridge::ChromeSyncNotificationBridge(
    const Profile* profile,
    const scoped_refptr<base::SequencedTaskRunner>& sync_task_runner)
    : sync_task_runner_(sync_task_runner),
      core_(new Core(sync_task_runner_)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile);
  registrar_.Add(this, chrome::NOTIFICATION_SYNC_REFRESH_LOCAL,
                 content::Source<Profile>(profile));
  registrar_.Add(this, chrome::NOTIFICATION_SYNC_REFRESH_REMOTE,
                 content::Source<Profile>(profile));

  if (!sync_task_runner_->PostTask(
          FROM_HERE, base::Bind(&Core::InitializeOnSyncThread, core_))) {
    NOTREACHED();
  }
}

ChromeSyncNotificationBridge::~ChromeSyncNotificationBridge() {}

void ChromeSyncNotificationBridge::StopForShutdown() {
  if (!sync_task_runner_->PostTask(
          FROM_HERE, base::Bind(&Core::CleanupOnSyncThread, core_))) {
    NOTREACHED();
  }
}

void ChromeSyncNotificationBridge::UpdateEnabledTypes(
    syncer::ModelTypeSet types) {
  DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
  core_->UpdateEnabledTypes(types);
}

void ChromeSyncNotificationBridge::RegisterHandler(
    syncer::SyncNotifierObserver* handler) {
  DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
  core_->RegisterHandler(handler);
}

void ChromeSyncNotificationBridge::UpdateRegisteredIds(
    syncer::SyncNotifierObserver* handler,
    const syncer::ObjectIdSet& ids) {
  DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
  core_->UpdateRegisteredIds(handler, ids);
}

void ChromeSyncNotificationBridge::UnregisterHandler(
    syncer::SyncNotifierObserver* handler) {
  DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
  core_->UnregisterHandler(handler);
}

void ChromeSyncNotificationBridge::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  syncer::IncomingNotificationSource notification_source;
  if (type == chrome::NOTIFICATION_SYNC_REFRESH_LOCAL) {
    notification_source = syncer::LOCAL_NOTIFICATION;
  } else if (type == chrome::NOTIFICATION_SYNC_REFRESH_REMOTE) {
    notification_source = syncer::REMOTE_NOTIFICATION;
  } else {
    NOTREACHED() << "Unexpected notification type: " << type;
    return;
  }

  content::Details<const syncer::ModelTypeStateMap>
      state_details(details);
  const syncer::ModelTypeStateMap& state_map = *(state_details.ptr());
  if (!sync_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&Core::EmitNotification,
                     core_, state_map, notification_source))) {
    NOTREACHED();
  }
}

}  // namespace browser_sync

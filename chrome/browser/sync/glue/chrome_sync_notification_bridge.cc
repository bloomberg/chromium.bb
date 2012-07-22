// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/chrome_sync_notification_bridge.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/observer_list.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "sync/notifier/sync_notifier_observer.h"

using content::BrowserThread;

namespace browser_sync {

class ChromeSyncNotificationBridge::Core
    : public base::RefCountedThreadSafe<Core> {
 public:
  // Created on UI thread.
  explicit Core(
      const scoped_refptr<base::SequencedTaskRunner>& sync_task_runner);

  // All member functions below must be called on the sync task runner.

  void AddObserver(syncer::SyncNotifierObserver* observer);
  void RemoveObserver(syncer::SyncNotifierObserver* observer);

  void EmitNotification(
      syncer::ModelTypePayloadMap payload_map,
      syncer::IncomingNotificationSource notification_source);

 private:
  friend class base::RefCountedThreadSafe<Core>;

  // Destroyed on the UI thread or on |sync_task_runner_|.
  ~Core();

  const scoped_refptr<base::SequencedTaskRunner> sync_task_runner_;

  // Used only on |sync_task_runner_|.
  syncer::ModelTypeSet enabled_types_;
  ObserverList<syncer::SyncNotifierObserver> observers_;
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
}

void ChromeSyncNotificationBridge::Core::AddObserver(
    syncer::SyncNotifierObserver* observer) {
  DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
  observers_.AddObserver(observer);
}

void ChromeSyncNotificationBridge::Core::RemoveObserver(
    syncer::SyncNotifierObserver* observer) {
  DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
  observers_.RemoveObserver(observer);
}

void ChromeSyncNotificationBridge::Core::EmitNotification(
    syncer::ModelTypePayloadMap payload_map,
    syncer::IncomingNotificationSource notification_source) {
  DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
  FOR_EACH_OBSERVER(
      syncer::SyncNotifierObserver, observers_,
      OnIncomingNotification(payload_map, notification_source));
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
}

ChromeSyncNotificationBridge::~ChromeSyncNotificationBridge() {}

void ChromeSyncNotificationBridge::UpdateEnabledTypes(
    const syncer::ModelTypeSet types) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  enabled_types_ = types;
}

void ChromeSyncNotificationBridge::AddObserver(
    syncer::SyncNotifierObserver* observer) {
  DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
  core_->AddObserver(observer);
}

void ChromeSyncNotificationBridge::RemoveObserver(
    syncer::SyncNotifierObserver* observer) {
  DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
  core_->RemoveObserver(observer);
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

  content::Details<const syncer::ModelTypePayloadMap>
      payload_details(details);
  syncer::ModelTypePayloadMap payload_map = *(payload_details.ptr());

  if (payload_map.empty()) {
    // No model types to invalidate, invalidating all enabled types.
    payload_map =
        syncer::ModelTypePayloadMapFromEnumSet(enabled_types_, std::string());
  }

  sync_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&Core::EmitNotification,
                 core_, payload_map, notification_source));
}

}  // namespace browser_sync

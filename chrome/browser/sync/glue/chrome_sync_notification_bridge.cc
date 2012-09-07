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
#include "sync/internal_api/public/base/model_type.h"
#include "sync/notifier/invalidation_handler.h"
#include "sync/notifier/invalidator_registrar.h"

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

  void RegisterHandler(syncer::InvalidationHandler* handler);
  void UpdateRegisteredIds(syncer::InvalidationHandler* handler,
                           const syncer::ObjectIdSet& ids);
  void UnregisterHandler(syncer::InvalidationHandler* handler);

  void EmitInvalidation(
      const syncer::ObjectIdStateMap& state_map,
      syncer::IncomingInvalidationSource invalidation_source);

  bool IsHandlerRegisteredForTest(syncer::InvalidationHandler* handler) const;
  syncer::ObjectIdSet GetRegisteredIdsForTest(
      syncer::InvalidationHandler* handler) const;

 private:
  friend class base::RefCountedThreadSafe<Core>;

  // Destroyed on the UI thread or on |sync_task_runner_|.
  ~Core();

  const scoped_refptr<base::SequencedTaskRunner> sync_task_runner_;

  // Used only on |sync_task_runner_|.
  scoped_ptr<syncer::InvalidatorRegistrar> invalidator_registrar_;
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
  DCHECK(!invalidator_registrar_.get());
}

void ChromeSyncNotificationBridge::Core::InitializeOnSyncThread() {
  invalidator_registrar_.reset(new syncer::InvalidatorRegistrar());
}

void ChromeSyncNotificationBridge::Core::CleanupOnSyncThread() {
  invalidator_registrar_.reset();
}

void ChromeSyncNotificationBridge::Core::RegisterHandler(
    syncer::InvalidationHandler* handler) {
  DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
  invalidator_registrar_->RegisterHandler(handler);
}

void ChromeSyncNotificationBridge::Core::UpdateRegisteredIds(
    syncer::InvalidationHandler* handler,
    const syncer::ObjectIdSet& ids) {
  DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
  invalidator_registrar_->UpdateRegisteredIds(handler, ids);
}

void ChromeSyncNotificationBridge::Core::UnregisterHandler(
    syncer::InvalidationHandler* handler) {
  DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
  invalidator_registrar_->UnregisterHandler(handler);
}

void ChromeSyncNotificationBridge::Core::EmitInvalidation(
    const syncer::ObjectIdStateMap& state_map,
    syncer::IncomingInvalidationSource invalidation_source) {
  DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
  const syncer::ObjectIdStateMap& effective_state_map =
      state_map.empty() ?
      ObjectIdSetToStateMap(
          invalidator_registrar_->GetAllRegisteredIds(), std::string()) :
      state_map;

  invalidator_registrar_->DispatchInvalidationsToHandlers(
      effective_state_map, invalidation_source);
}

bool ChromeSyncNotificationBridge::Core::IsHandlerRegisteredForTest(
    syncer::InvalidationHandler* handler) const {
  DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
  return invalidator_registrar_->IsHandlerRegisteredForTest(handler);
}

syncer::ObjectIdSet
ChromeSyncNotificationBridge::Core::GetRegisteredIdsForTest(
    syncer::InvalidationHandler* handler) const {
  DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
  return invalidator_registrar_->GetRegisteredIds(handler);
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

void ChromeSyncNotificationBridge::RegisterHandler(
    syncer::InvalidationHandler* handler) {
  DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
  core_->RegisterHandler(handler);
}

void ChromeSyncNotificationBridge::UpdateRegisteredIds(
    syncer::InvalidationHandler* handler,
    const syncer::ObjectIdSet& ids) {
  DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
  core_->UpdateRegisteredIds(handler, ids);
}

void ChromeSyncNotificationBridge::UnregisterHandler(
    syncer::InvalidationHandler* handler) {
  DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
  core_->UnregisterHandler(handler);
}

bool ChromeSyncNotificationBridge::IsHandlerRegisteredForTest(
    syncer::InvalidationHandler* handler) const {
  DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
  return core_->IsHandlerRegisteredForTest(handler);
}

syncer::ObjectIdSet ChromeSyncNotificationBridge::GetRegisteredIdsForTest(
    syncer::InvalidationHandler* handler) const {
  DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
  return core_->GetRegisteredIdsForTest(handler);
}

void ChromeSyncNotificationBridge::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  syncer::IncomingInvalidationSource invalidation_source;
  if (type == chrome::NOTIFICATION_SYNC_REFRESH_LOCAL) {
    invalidation_source = syncer::LOCAL_INVALIDATION;
  } else if (type == chrome::NOTIFICATION_SYNC_REFRESH_REMOTE) {
    invalidation_source = syncer::REMOTE_INVALIDATION;
  } else {
    NOTREACHED() << "Unexpected invalidation type: " << type;
    return;
  }

  // TODO(akalin): Use ObjectIdStateMap here instead.  We'll have to
  // make sure all emitters of the relevant notifications also use
  // ObjectIdStateMap.
  content::Details<const syncer::ModelTypeStateMap>
      state_details(details);
  const syncer::ModelTypeStateMap& state_map = *(state_details.ptr());
  if (!sync_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&Core::EmitInvalidation,
                     core_,
                     ModelTypeStateMapToObjectIdStateMap(state_map),
                     invalidation_source))) {
    NOTREACHED();
  }
}

}  // namespace browser_sync

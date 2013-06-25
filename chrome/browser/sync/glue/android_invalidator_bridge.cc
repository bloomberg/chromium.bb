// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/android_invalidator_bridge.h"

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

class AndroidInvalidatorBridge::Core
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
      const syncer::ObjectIdInvalidationMap& invalidation_map);

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

AndroidInvalidatorBridge::Core::Core(
    const scoped_refptr<base::SequencedTaskRunner>& sync_task_runner)
    : sync_task_runner_(sync_task_runner) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(sync_task_runner_.get());
}

AndroidInvalidatorBridge::Core::~Core() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         sync_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(!invalidator_registrar_.get());
}

void AndroidInvalidatorBridge::Core::InitializeOnSyncThread() {
  invalidator_registrar_.reset(new syncer::InvalidatorRegistrar());
}

void AndroidInvalidatorBridge::Core::CleanupOnSyncThread() {
  invalidator_registrar_.reset();
}

void AndroidInvalidatorBridge::Core::RegisterHandler(
    syncer::InvalidationHandler* handler) {
  DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
  invalidator_registrar_->RegisterHandler(handler);
}

void AndroidInvalidatorBridge::Core::UpdateRegisteredIds(
    syncer::InvalidationHandler* handler,
    const syncer::ObjectIdSet& ids) {
  DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
  invalidator_registrar_->UpdateRegisteredIds(handler, ids);
}

void AndroidInvalidatorBridge::Core::UnregisterHandler(
    syncer::InvalidationHandler* handler) {
  DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
  invalidator_registrar_->UnregisterHandler(handler);
}

void AndroidInvalidatorBridge::Core::EmitInvalidation(
    const syncer::ObjectIdInvalidationMap& invalidation_map) {
  DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
  const syncer::ObjectIdInvalidationMap& effective_invalidation_map =
      invalidation_map.empty() ?
      ObjectIdSetToInvalidationMap(
          invalidator_registrar_->GetAllRegisteredIds(), std::string()) :
      invalidation_map;

  invalidator_registrar_->DispatchInvalidationsToHandlers(
      effective_invalidation_map);
}

bool AndroidInvalidatorBridge::Core::IsHandlerRegisteredForTest(
    syncer::InvalidationHandler* handler) const {
  DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
  return invalidator_registrar_->IsHandlerRegisteredForTest(handler);
}

syncer::ObjectIdSet
AndroidInvalidatorBridge::Core::GetRegisteredIdsForTest(
    syncer::InvalidationHandler* handler) const {
  DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
  return invalidator_registrar_->GetRegisteredIds(handler);
}

AndroidInvalidatorBridge::AndroidInvalidatorBridge(
    const Profile* profile,
    const scoped_refptr<base::SequencedTaskRunner>& sync_task_runner)
    : sync_task_runner_(sync_task_runner),
      core_(new Core(sync_task_runner_)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile);
  registrar_.Add(this, chrome::NOTIFICATION_SYNC_REFRESH_REMOTE,
                 content::Source<Profile>(profile));

  if (!sync_task_runner_->PostTask(
          FROM_HERE, base::Bind(&Core::InitializeOnSyncThread, core_))) {
    NOTREACHED();
  }
}

AndroidInvalidatorBridge::~AndroidInvalidatorBridge() {}

void AndroidInvalidatorBridge::StopForShutdown() {
  if (!sync_task_runner_->PostTask(
          FROM_HERE, base::Bind(&Core::CleanupOnSyncThread, core_))) {
    NOTREACHED();
  }
}

void AndroidInvalidatorBridge::RegisterHandler(
    syncer::InvalidationHandler* handler) {
  DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
  core_->RegisterHandler(handler);
}

void AndroidInvalidatorBridge::UpdateRegisteredIds(
    syncer::InvalidationHandler* handler,
    const syncer::ObjectIdSet& ids) {
  DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
  core_->UpdateRegisteredIds(handler, ids);
}

void AndroidInvalidatorBridge::UnregisterHandler(
    syncer::InvalidationHandler* handler) {
  DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
  core_->UnregisterHandler(handler);
}

void AndroidInvalidatorBridge::Acknowledge(
    const invalidation::ObjectId& id, const syncer::AckHandle& ack_handle) {
  // Do nothing.
}

syncer::InvalidatorState AndroidInvalidatorBridge::GetInvalidatorState() const {
  return syncer::INVALIDATIONS_ENABLED;
}

void AndroidInvalidatorBridge::UpdateCredentials(
    const std::string& email, const std::string& token) { }

void AndroidInvalidatorBridge::SendInvalidation(
    const syncer::ObjectIdInvalidationMap& invalidation_map) { }

bool AndroidInvalidatorBridge::IsHandlerRegisteredForTest(
    syncer::InvalidationHandler* handler) const {
  DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
  return core_->IsHandlerRegisteredForTest(handler);
}

syncer::ObjectIdSet AndroidInvalidatorBridge::GetRegisteredIdsForTest(
    syncer::InvalidationHandler* handler) const {
  DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
  return core_->GetRegisteredIdsForTest(handler);
}

void AndroidInvalidatorBridge::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(type, chrome::NOTIFICATION_SYNC_REFRESH_REMOTE);

  // TODO(akalin): Use ObjectIdInvalidationMap here instead.  We'll have to
  // make sure all emitters of the relevant notifications also use
  // ObjectIdInvalidationMap.
  content::Details<const syncer::ModelTypeInvalidationMap>
      state_details(details);
  const syncer::ModelTypeInvalidationMap& invalidation_map =
      *(state_details.ptr());
  if (!sync_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&Core::EmitInvalidation,
                     core_,
                     ModelTypeInvalidationMapToObjectIdInvalidationMap(
                         invalidation_map)))) {
    NOTREACHED();
  }
}

}  // namespace browser_sync

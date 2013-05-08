// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_ANDROID_INVALIDATOR_BRIDGE_H_
#define CHROME_BROWSER_SYNC_GLUE_ANDROID_INVALIDATOR_BRIDGE_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "sync/notifier/invalidator.h"

class Profile;

namespace syncer {
class InvalidationHandler;
}  // namespace

namespace browser_sync {

// A bridge that converts Chrome events on the UI thread to sync
// notifications on the sync task runner.
//
// Listens to NOTIFICATION_SYNC_REFRESH_REMOTE (on the UI thread) and triggers
// each observer's OnIncomingNotification method on these notifications (on the
// sync task runner).  Android clients receive invalidations through this
// mechanism exclusively, hence the name.
class AndroidInvalidatorBridge
    : public content::NotificationObserver, syncer::Invalidator {
 public:
  // Must be created and destroyed on the UI thread.
  AndroidInvalidatorBridge(
      const Profile* profile,
      const scoped_refptr<base::SequencedTaskRunner>& sync_task_runner);
  virtual ~AndroidInvalidatorBridge();

  // Must be called on the UI thread while the sync task runner is still
  // around.  No other member functions on the sync thread may be called after
  // this is called.
  void StopForShutdown();

  // Invalidator implementation.  Must be called on the sync task runner.
  virtual void RegisterHandler(syncer::InvalidationHandler* handler) OVERRIDE;
  virtual void UpdateRegisteredIds(syncer::InvalidationHandler* handler,
                                   const syncer::ObjectIdSet& ids) OVERRIDE;
  virtual void UnregisterHandler(syncer::InvalidationHandler* handler) OVERRIDE;
  virtual void Acknowledge(const invalidation::ObjectId& id,
                           const syncer::AckHandle& ack_handle) OVERRIDE;
  virtual syncer::InvalidatorState GetInvalidatorState() const OVERRIDE;

  // The following members of the Invalidator interface are not applicable to
  // this invalidator and are implemented as no-ops.
  virtual void UpdateCredentials(
      const std::string& email, const std::string& token) OVERRIDE;
  virtual void SendInvalidation(
      const syncer::ObjectIdInvalidationMap& invalidation_map) OVERRIDE;

  bool IsHandlerRegisteredForTest(
      syncer::InvalidationHandler* handler) const;
  syncer::ObjectIdSet GetRegisteredIdsForTest(
      syncer::InvalidationHandler* handler) const;

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

#endif  // CHROME_BROWSER_SYNC_GLUE_ANDROID_INVALIDATOR_BRIDGE_H_

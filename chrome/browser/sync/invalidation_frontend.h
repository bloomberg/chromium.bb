// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_INVALIDATION_FRONTEND_H_
#define CHROME_BROWSER_SYNC_INVALIDATION_FRONTEND_H_

#include "sync/notifier/invalidation_util.h"

namespace syncer {
class SyncNotifierObserver;
}  // namespace syncer

// Interface for classes that handle invalidation registrations and send out
// invalidations to register handlers.
class InvalidationFrontend {
 public:
  // Starts sending notifications to |handler|.  |handler| must not be NULL,
  // and it must not already be registered.
  //
  // Handler registrations are persisted across restarts of sync.
  virtual void RegisterInvalidationHandler(
      syncer::SyncNotifierObserver* handler) = 0;

  // Updates the set of ObjectIds associated with |handler|.  |handler| must
  // not be NULL, and must already be registered.  An ID must be registered for
  // at most one handler.
  //
  // Registered IDs are persisted across restarts of sync.
  virtual void UpdateRegisteredInvalidationIds(
      syncer::SyncNotifierObserver* handler,
      const syncer::ObjectIdSet& ids) = 0;

  // Stops sending notifications to |handler|.  |handler| must not be NULL, and
  // it must already be registered.  Note that this doesn't unregister the IDs
  // associated with |handler|.
  //
  // Handler registrations are persisted across restarts of sync.
  virtual void UnregisterInvalidationHandler(
      syncer::SyncNotifierObserver* handler) = 0;

 protected:
  virtual ~InvalidationFrontend() { }
};

#endif  // CHROME_BROWSER_SYNC_INVALIDATION_FRONTEND_H_

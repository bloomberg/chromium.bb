// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_INVALIDATION_FRONTEND_H_
#define CHROME_BROWSER_SYNC_INVALIDATION_FRONTEND_H_

#include "sync/notifier/invalidation_util.h"

namespace syncer {
class InvalidationHandler;
}  // namespace syncer

// Interface for classes that handle invalidation registrations and send out
// invalidations to register handlers.
//
// Invalidation clients should follow the pattern below:
//
// When starting the client:
//
//   frontend->RegisterInvalidationHandler(client_handler);
//
// When the set of IDs to register changes for the client during its lifetime
// (i.e., between calls to RegisterInvalidationHandler(client_handler) and
// UnregisterInvalidationHandler(client_handler):
//
//   frontend->UpdateRegisteredInvalidationIds(client_handler, client_ids);
//
// When shutting down the client for browser shutdown:
//
//   frontend->UnregisterInvalidationHandler(client_handler);
//
// Note that there's no call to UpdateRegisteredIds() -- this is because the
// invalidation API persists registrations across browser restarts.
//
// When permanently shutting down the client, e.g. when disabling the related
// feature:
//
//   frontend->UpdateRegisteredInvalidationIds(client_handler, ObjectIdSet());
//   frontend->UnregisterInvalidationHandler(client_handler);
//
// If an invalidation handler cares about the invalidator state, it should also
// do the following when starting the client:
//
//   invalidator_state = frontend->GetInvalidatorState();
//
// It can also do the above in OnInvalidatorStateChange(), or it can use the
// argument to OnInvalidatorStateChange().
//
// NOTE(akalin): Invalidations that come in during browser shutdown may get
// dropped.  This won't matter once we have an Acknowledge API, though: see
// http://crbug.com/78462 and http://crbug.com/124149.
class InvalidationFrontend {
 public:
  // Starts sending notifications to |handler|.  |handler| must not be NULL,
  // and it must not already be registered.
  //
  // Handler registrations are persisted across restarts of sync.
  virtual void RegisterInvalidationHandler(
      syncer::InvalidationHandler* handler) = 0;

  // Updates the set of ObjectIds associated with |handler|.  |handler| must
  // not be NULL, and must already be registered.  An ID must be registered for
  // at most one handler.
  //
  // Registered IDs are persisted across restarts of sync.
  virtual void UpdateRegisteredInvalidationIds(
      syncer::InvalidationHandler* handler,
      const syncer::ObjectIdSet& ids) = 0;

  // Stops sending notifications to |handler|.  |handler| must not be NULL, and
  // it must already be registered.  Note that this doesn't unregister the IDs
  // associated with |handler|.
  //
  // Handler registrations are persisted across restarts of sync.
  virtual void UnregisterInvalidationHandler(
      syncer::InvalidationHandler* handler) = 0;

  // Returns the current invalidator state.  When called from within
  // InvalidationHandler::OnInvalidatorStateChange(), this must return
  // the updated state.
  virtual syncer::InvalidatorState GetInvalidatorState() const = 0;

 protected:
  virtual ~InvalidationFrontend() { }
};

#endif  // CHROME_BROWSER_SYNC_INVALIDATION_FRONTEND_H_

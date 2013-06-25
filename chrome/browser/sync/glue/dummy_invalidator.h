// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_DUMMY_INVALIDATOR_H_
#define CHROME_BROWSER_SYNC_GLUE_DUMMY_INVALIDATOR_H_

#include "sync/notifier/invalidator.h"

// A fake invalidator that does nothing, and stays in a "transient" error state.
// This is useful for cases where we know that invalidations won't work, but
// still want to keep Sync running without them
class DummyInvalidator : public syncer::Invalidator {
 public:
  DummyInvalidator();
  virtual ~DummyInvalidator();

  virtual void RegisterHandler(syncer::InvalidationHandler* handler) OVERRIDE;
  virtual void UpdateRegisteredIds(syncer::InvalidationHandler* handler,
                                   const syncer::ObjectIdSet& ids) OVERRIDE;
  virtual void UnregisterHandler(syncer::InvalidationHandler* handler) OVERRIDE;
  virtual void Acknowledge(const invalidation::ObjectId& id,
                           const syncer::AckHandle& ack_handle) OVERRIDE;
  virtual syncer::InvalidatorState GetInvalidatorState() const OVERRIDE;
  virtual void UpdateCredentials(
      const std::string& email, const std::string& token) OVERRIDE;
  virtual void SendInvalidation(
      const syncer::ObjectIdInvalidationMap& invalidation_map) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyInvalidator);
};

#endif  // CHROME_BROWSER_SYNC_GLUE_DUMMY_INVALIDATOR_H_

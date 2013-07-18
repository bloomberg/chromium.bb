// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INVALIDATION_FAKE_INVALIDATION_SERVICE_H_
#define CHROME_BROWSER_INVALIDATION_FAKE_INVALIDATION_SERVICE_H_

#include "base/basictypes.h"
#include "chrome/browser/invalidation/invalidation_service.h"
#include "sync/notifier/invalidator_registrar.h"

namespace invalidation {

// An InvalidationService that emits invalidations only when
// its EmitInvalidationForTest method is called.
class FakeInvalidationService : public InvalidationService {
 public:
  FakeInvalidationService();
  virtual ~FakeInvalidationService();

  virtual void RegisterInvalidationHandler(
      syncer::InvalidationHandler* handler) OVERRIDE;
  virtual void UpdateRegisteredInvalidationIds(
      syncer::InvalidationHandler* handler,
      const syncer::ObjectIdSet& ids) OVERRIDE;
  virtual void UnregisterInvalidationHandler(
      syncer::InvalidationHandler* handler) OVERRIDE;

  virtual void AcknowledgeInvalidation(
      const invalidation::ObjectId& id,
      const syncer::AckHandle& ack_handle) OVERRIDE;

  virtual syncer::InvalidatorState GetInvalidatorState() const OVERRIDE;
  virtual std::string GetInvalidatorClientId() const OVERRIDE;

  void EmitInvalidationForTest(
      const invalidation::ObjectId& object_id,
      int64 version,
      const std::string& payload);

 private:
  std::string client_id_;
  syncer::InvalidatorRegistrar invalidator_registrar_;

  DISALLOW_COPY_AND_ASSIGN(FakeInvalidationService);
};

}  // namespace invalidation

#endif  // CHROME_BROWSER_INVALIDATION_FAKE_INVALIDATION_SERVICE_H_

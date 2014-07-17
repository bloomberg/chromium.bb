// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INVALIDATION_FAKE_INVALIDATION_SERVICE_H_
#define CHROME_BROWSER_INVALIDATION_FAKE_INVALIDATION_SERVICE_H_

#include <list>
#include <utility>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service.h"
#include "components/invalidation/invalidation_service.h"
#include "components/invalidation/invalidator_registrar.h"
#include "components/invalidation/mock_ack_handler.h"
#include "google_apis/gaia/fake_identity_provider.h"

namespace syncer {
class Invalidation;
}

namespace invalidation {

class InvalidationLogger;

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

  virtual syncer::InvalidatorState GetInvalidatorState() const OVERRIDE;
  virtual std::string GetInvalidatorClientId() const OVERRIDE;
  virtual InvalidationLogger* GetInvalidationLogger() OVERRIDE;
  virtual void RequestDetailedStatus(
      base::Callback<void(const base::DictionaryValue&)> caller) const OVERRIDE;
  virtual IdentityProvider* GetIdentityProvider() OVERRIDE;

  void SetInvalidatorState(syncer::InvalidatorState state);

  const syncer::InvalidatorRegistrar& invalidator_registrar() const {
    return invalidator_registrar_;
  }

  void EmitInvalidationForTest(const syncer::Invalidation& invalidation);

  // Emitted invalidations will be hooked up to this AckHandler.  Clients can
  // query it to assert the invalidaitons are being acked properly.
  syncer::MockAckHandler* GetMockAckHandler();

 private:
  std::string client_id_;
  syncer::InvalidatorRegistrar invalidator_registrar_;
  syncer::MockAckHandler mock_ack_handler_;
  FakeProfileOAuth2TokenService token_service_;
  FakeIdentityProvider identity_provider_;

  DISALLOW_COPY_AND_ASSIGN(FakeInvalidationService);
};

}  // namespace invalidation

#endif  // CHROME_BROWSER_INVALIDATION_FAKE_INVALIDATION_SERVICE_H_

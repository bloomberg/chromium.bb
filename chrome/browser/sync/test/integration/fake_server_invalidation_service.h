// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_FAKE_SERVER_INVALIDATION_SERVICE_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_FAKE_SERVER_INVALIDATION_SERVICE_H_

#include <string>
#include <utility>

#include "base/basictypes.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service.h"
#include "components/invalidation/invalidation_service.h"
#include "components/invalidation/invalidator_registrar.h"
#include "google_apis/gaia/fake_identity_provider.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/test/fake_server/fake_server.h"

namespace invalidation {
class InvalidationLogger;
}

namespace fake_server {

// An InvalidationService that is used in conjunction with FakeServer.
class FakeServerInvalidationService : public invalidation::InvalidationService,
                                      public FakeServer::Observer {
 public:
  FakeServerInvalidationService();
  virtual ~FakeServerInvalidationService();

  virtual void RegisterInvalidationHandler(
      syncer::InvalidationHandler* handler) OVERRIDE;
  virtual void UpdateRegisteredInvalidationIds(
      syncer::InvalidationHandler* handler,
      const syncer::ObjectIdSet& ids) OVERRIDE;
  virtual void UnregisterInvalidationHandler(
      syncer::InvalidationHandler* handler) OVERRIDE;

  virtual syncer::InvalidatorState GetInvalidatorState() const OVERRIDE;
  virtual std::string GetInvalidatorClientId() const OVERRIDE;
  virtual invalidation::InvalidationLogger* GetInvalidationLogger() OVERRIDE;
  virtual void RequestDetailedStatus(
      base::Callback<void(const base::DictionaryValue&)> caller) const OVERRIDE;
  virtual IdentityProvider* GetIdentityProvider() OVERRIDE;

  // Functions to enable or disable sending of self-notifications.  In the real
  // world, clients do not receive notifications of their own commits.
  void EnableSelfNotifications();
  void DisableSelfNotifications();

  // FakeServer::Observer:
  virtual void OnCommit(
      const std::string& committer_id,
      syncer::ModelTypeSet committed_model_types) OVERRIDE;

 private:
  std::string client_id_;
  bool self_notify_;

  syncer::InvalidatorRegistrar invalidator_registrar_;
  FakeProfileOAuth2TokenService token_service_;
  FakeIdentityProvider identity_provider_;

  DISALLOW_COPY_AND_ASSIGN(FakeServerInvalidationService);
};

}  // namespace fake_server

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_FAKE_SERVER_INVALIDATION_SERVICE_H_

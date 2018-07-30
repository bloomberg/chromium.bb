// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_FCM_INVALIDATION_SERVICE_H_
#define COMPONENTS_INVALIDATION_IMPL_FCM_INVALIDATION_SERVICE_H_

#include "base/macros.h"
#include "base/timer/timer.h"
#include "components/invalidation/impl/invalidation_logger.h"
#include "components/invalidation/impl/invalidator_registrar.h"
#include "components/invalidation/public/identity_provider.h"
#include "components/invalidation/public/invalidation_handler.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "net/base/backoff_entry.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace gcm {
class GCMDriver;
}

class PrefService;

namespace syncer {
class Invalidator;
}

namespace instance_id {
class InstanceIDDriver;
}

namespace invalidation {

// This InvalidationService wraps the C++ Invalidation Client (FCM) library.
// It provides invalidations for desktop platforms (Win, Mac, Linux).
class FCMInvalidationService : public InvalidationService,
                               public IdentityProvider::Observer,
                               public syncer::InvalidationHandler {
 public:
  FCMInvalidationService(std::unique_ptr<IdentityProvider> identity_provider,
                         gcm::GCMDriver* gcm_driver,
                         instance_id::InstanceIDDriver* instance_id_driver,
                         PrefService* pref_service,
                         const syncer::ParseJSONCallback& parse_json,
                         network::mojom::URLLoaderFactory* loader_factory);
  ~FCMInvalidationService() override;

  void Init();

  // InvalidationService implementation.
  // It is an error to have registered handlers when the service is destroyed.
  void RegisterInvalidationHandler(
      syncer::InvalidationHandler* handler) override;
  bool UpdateRegisteredInvalidationIds(syncer::InvalidationHandler* handler,
                                       const syncer::ObjectIdSet& ids) override;
  void UnregisterInvalidationHandler(
      syncer::InvalidationHandler* handler) override;
  syncer::InvalidatorState GetInvalidatorState() const override;
  std::string GetInvalidatorClientId() const override;
  InvalidationLogger* GetInvalidationLogger() override;
  void RequestDetailedStatus(
      base::RepeatingCallback<void(const base::DictionaryValue&)> caller)
      const override;

  void RequestAccessToken();

  void OnAccessTokenRequestCompleted(GoogleServiceAuthError error,
                                     std::string access_token);
  void OnAccessTokenRequestSucceeded(std::string access_token);
  void OnAccessTokenRequestFailed(GoogleServiceAuthError error);

  // IdentityProvider::Observer implementation.
  void OnActiveAccountRefreshTokenUpdated() override;
  void OnActiveAccountRefreshTokenRemoved() override;
  void OnActiveAccountLogin() override;
  void OnActiveAccountLogout() override;

  // syncer::InvalidationHandler implementation.
  void OnInvalidatorStateChange(syncer::InvalidatorState state) override;
  void OnIncomingInvalidation(
      const syncer::ObjectIdInvalidationMap& invalidation_map) override;
  std::string GetOwnerName() const override;

 protected:
  friend class FCMInvalidationServiceTestDelegate;

  // Initializes with an injected invalidator.
  void InitForTest(syncer::Invalidator* invalidator);

 private:
  bool IsReadyToStart();
  bool IsStarted() const;

  void StartInvalidator();
  void StopInvalidator();
  void UpdateInvalidatorCredentials();

  std::unique_ptr<IdentityProvider> identity_provider_;
  // FCMInvalidationService needs to hold reference to access_token_fetcher_
  // for the duration of request in order to receive callbacks.
  std::unique_ptr<ActiveAccountAccessTokenFetcher> access_token_fetcher_;
  base::OneShotTimer request_access_token_retry_timer_;
  net::BackoffEntry request_access_token_backoff_;

  syncer::InvalidatorRegistrar invalidator_registrar_;
  std::unique_ptr<syncer::Invalidator> invalidator_;

  // FCMInvalidationService needs to remember access token in order to
  // invalidate it with IdentityProvider.
  std::string access_token_;

  // The invalidation logger object we use to record state changes
  // and invalidations.
  InvalidationLogger logger_;

  gcm::GCMDriver* gcm_driver_;
  instance_id::InstanceIDDriver* instance_id_driver_;

  PrefService* pref_service_;
  syncer::ParseJSONCallback parse_json_;
  network::mojom::URLLoaderFactory* loader_factory_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(FCMInvalidationService);
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_IMPL_FCM_INVALIDATION_SERVICE_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_PER_USER_TOPIC_REGISTRATION_MANAGER_H_
#define COMPONENTS_INVALIDATION_IMPL_PER_USER_TOPIC_REGISTRATION_MANAGER_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/invalidation/impl/per_user_topic_registration_request.h"
#include "components/invalidation/public/identity_provider.h"
#include "components/invalidation/public/invalidation_export.h"
#include "components/invalidation/public/invalidation_object_id.h"
#include "components/invalidation/public/invalidation_util.h"
#include "net/base/backoff_entry.h"
#include "net/url_request/url_request_context_getter.h"

class PrefRegistrySimple;
class PrefService;

namespace invalidation {
class ActiveAccountAccessTokenFetcher;
class IdentityProvider;
}  // namespace invalidation

namespace syncer {

// A class that manages the registration of types for server-issued
// notifications.
// Manages the details of registering types for invalidation. For example,
// Chrome Sync uses the ModelTypes (bookmarks, passwords, autofill data) as
// topics, which will be registered for the invalidations.
// TODO(melandory): Methods in this class have names which are similar to names
// in RegistrationManager. As part of clean-up work for removing old
// RegistrationManger and cachinvalidation library it's worth to revisit methods
// names in this class.
class INVALIDATION_EXPORT PerUserTopicRegistrationManager {
 public:
  PerUserTopicRegistrationManager(
      invalidation::IdentityProvider* identity_provider,
      PrefService* local_state,
      network::mojom::URLLoaderFactory* url_loader_factory,
      const ParseJSONCallback& parse_json);

  virtual ~PerUserTopicRegistrationManager();

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  virtual void UpdateRegisteredIds(const InvalidationObjectIdSet& ids,
                                   const std::string& token);

  InvalidationObjectIdSet GetRegisteredIds() const;

 private:
  struct RegistrationEntry;

  void DoRegistrationUpdate();

  // Tries to register |id|. No retry in case of failure.
  void StartRegistrationRequest(const invalidation::InvalidationObjectId& id);

  // Unregisters the given object ID.
  void UnregisterId(const invalidation::InvalidationObjectId& id);

  void RequestAccessToken();

  void OnAccessTokenRequestCompleted(GoogleServiceAuthError error,
                                     std::string access_token);
  void OnAccessTokenRequestSucceeded(std::string access_token);
  void OnAccessTokenRequestFailed(GoogleServiceAuthError error);

  std::map<invalidation::InvalidationObjectId,
           std::unique_ptr<RegistrationEntry>,
           InvalidationObjectIdLessThan>
      registration_statuses_;

  // Token derrived from GCM IID.
  std::string token_;

  PrefService* local_state_ = nullptr;

  invalidation::IdentityProvider* const identity_provider_;
  std::string access_token_;
  std::unique_ptr<invalidation::ActiveAccountAccessTokenFetcher>
      access_token_fetcher_;
  base::OneShotTimer request_access_token_retry_timer_;
  net::BackoffEntry request_access_token_backoff_;

  // The callback for Parsing JSON.
  ParseJSONCallback parse_json_;
  network::mojom::URLLoaderFactory* url_loader_factory_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(PerUserTopicRegistrationManager);
};

}  // namespace syncer

#endif  // COMPONENTS_INVALIDATION_IMPL_PER_USER_TOPIC_REGISTRATION_MANAGER_H_

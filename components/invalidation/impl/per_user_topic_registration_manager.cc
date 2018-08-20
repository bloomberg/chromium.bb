// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/per_user_topic_registration_manager.h"

#include <stdint.h>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/invalidation/public/identity_provider.h"
#include "components/invalidation/public/invalidation_object_id.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace syncer {

namespace {

const char kTypeRegisteredForInvalidation[] =
    "invalidation.registered_for_invalidation";

const char kInvalidationRegistrationScope[] =
    "https://firebaseperusertopics-pa.googleapis.com";

const char kProjectId[] = "8181035976";

const char kFCMOAuthScope[] =
    "https://www.googleapis.com/auth/firebase.messaging";

static const net::BackoffEntry::Policy kRequestAccessTokenBackoffPolicy = {
    // Number of initial errors (in sequence) to ignore before applying
    // exponential back-off rules.
    0,

    // Initial delay for exponential back-off in ms.
    2000,

    // Factor by which the waiting time will be multiplied.
    2,

    // Fuzzing percentage. ex: 10% will spread requests randomly
    // between 90%-100% of the calculated time.
    0.2,  // 20%

    // Maximum amount of time we are willing to delay our request in ms.
    1000 * 3600 * 4,  // 4 hours.

    // Time to keep an entry from being discarded even when it
    // has no significant state, -1 to never discard.
    -1,

    // Don't use initial delay unless the last request was an error.
    false,
};

}  // namespace

// static
void PerUserTopicRegistrationManager::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kTypeRegisteredForInvalidation);
}

struct PerUserTopicRegistrationManager::RegistrationEntry {
  enum RegistrationState { REGISTERED, UNREGISTERED };
  RegistrationEntry(const invalidation::InvalidationObjectId& id,
                    PrefService* pref);
  ~RegistrationEntry();

  void RegistrationFinished(const Status& code,
                            const std::string& private_topic_name);

  void DoRegister();

  // The object for which this is the status.
  const invalidation::InvalidationObjectId id;

  // Whether this data type should be registered.  Set to false if
  // we get a non-transient registration failure.
  bool enabled;
  // The current registration state.
  RegistrationState state;

  std::string private_topic_name;

  PrefService* pref;

  std::unique_ptr<PerUserTopicRegistrationRequest> request;

  DISALLOW_COPY_AND_ASSIGN(RegistrationEntry);
};

PerUserTopicRegistrationManager::RegistrationEntry::RegistrationEntry(
    const invalidation::InvalidationObjectId& id,
    PrefService* pref)
    : id(id),
      enabled(true),
      state(RegistrationEntry::UNREGISTERED),
      pref(pref) {
  const base::DictionaryValue* registered_for_invalidation =
      pref->GetDictionary(kTypeRegisteredForInvalidation);
  if (registered_for_invalidation) {
    auto* value =
        registered_for_invalidation->FindKey(SerializeInvalidationObjectId(id));
    if (value) {
      value->GetAsString(&private_topic_name);
      state = RegistrationEntry::REGISTERED;
    }
  }
}

PerUserTopicRegistrationManager::RegistrationEntry::~RegistrationEntry() {}

void PerUserTopicRegistrationManager::RegistrationEntry::RegistrationFinished(
    const Status& code,
    const std::string& topic_name) {
  if (code.IsSuccess()) {
    private_topic_name = topic_name;
    state = RegistrationEntry::REGISTERED;
    DictionaryPrefUpdate update(pref, kTypeRegisteredForInvalidation);
    base::DictionaryValue* pref_data = update.Get();
    pref_data->SetString(SerializeInvalidationObjectId(id), private_topic_name);
  }
}

PerUserTopicRegistrationManager::PerUserTopicRegistrationManager(
    invalidation::IdentityProvider* identity_provider,
    PrefService* local_state,
    network::mojom::URLLoaderFactory* url_loader_factory,
    const ParseJSONCallback& parse_json)
    : local_state_(local_state),
      identity_provider_(identity_provider),
      request_access_token_backoff_(&kRequestAccessTokenBackoffPolicy),
      parse_json_(parse_json),
      url_loader_factory_(url_loader_factory) {}

PerUserTopicRegistrationManager::~PerUserTopicRegistrationManager() {}

void PerUserTopicRegistrationManager::UpdateRegisteredIds(
    const InvalidationObjectIdSet& ids,
    const std::string& instance_id_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  token_ = instance_id_token;
  // TODO(melandory): On change of token registrations
  // should be re-requested.
  for (const invalidation::InvalidationObjectId& objectId : ids) {
    if (!base::ContainsKey(registration_statuses_, objectId)) {
      registration_statuses_[objectId] =
          std::make_unique<RegistrationEntry>(objectId, local_state_);
    }
  }
  RequestAccessToken();
}

void PerUserTopicRegistrationManager::DoRegistrationUpdate() {
  for (const auto& registration_status : registration_statuses_) {
    if (registration_status.second->state == RegistrationEntry::UNREGISTERED) {
      StartRegistrationRequest(registration_status.first);
    }
  }
}

void PerUserTopicRegistrationManager::StartRegistrationRequest(
    const invalidation::InvalidationObjectId& id) {
  auto it = registration_statuses_.find(id);
  if (it == registration_statuses_.end()) {
    NOTREACHED() << "StartRegistrationRequest called on "
                 << InvalidationObjectIdToString(id)
                 << " which is not in the registration map";
    return;
  }
  PerUserTopicRegistrationRequest::Builder builder;

  it->second->request = builder.SetToken(token_)
                            .SetScope(kInvalidationRegistrationScope)
                            .SetPublicTopicName(id.name())
                            .SetAuthenticationHeader(base::StringPrintf(
                                "Bearer %s", access_token_.c_str()))
                            .SetProjectId(kProjectId)
                            .SetType(PerUserTopicRegistrationRequest::SUBSCRIBE)
                            .Build();

  it->second->request->Start(
      base::BindOnce(&PerUserTopicRegistrationManager::RegistrationEntry::
                         RegistrationFinished,
                     base::Unretained(it->second.get())),
      parse_json_, url_loader_factory_);
}

InvalidationObjectIdSet PerUserTopicRegistrationManager::GetRegisteredIds()
    const {
  InvalidationObjectIdSet ids;
  for (const auto& status_pair : registration_statuses_) {
    if (status_pair.second->state == RegistrationEntry::REGISTERED) {
      ids.insert(status_pair.first);
    }
  }
  return ids;
}

void PerUserTopicRegistrationManager::RequestAccessToken() {
  // TODO(melandory): Implement traffic optimisation.
  // * Before sending request to server ask for access token from identity
  //   provider (don't invalidate previous token).
  //   Identity provider will take care of retrieving/caching.
  // * Only invalidate access token when server didn't accept it.

  // Only one active request at a time.
  if (access_token_fetcher_ != nullptr)
    return;
  request_access_token_retry_timer_.Stop();
  OAuth2TokenService::ScopeSet oauth2_scopes = {kFCMOAuthScope};
  // Invalidate previous token, otherwise the identity provider will return the
  // same token again.
  identity_provider_->InvalidateAccessToken(oauth2_scopes, access_token_);
  access_token_.clear();
  access_token_fetcher_ = identity_provider_->FetchAccessToken(
      "fcm_invalidation", oauth2_scopes,
      base::BindOnce(
          &PerUserTopicRegistrationManager::OnAccessTokenRequestCompleted,
          base::Unretained(this)));
}

void PerUserTopicRegistrationManager::OnAccessTokenRequestCompleted(
    GoogleServiceAuthError error,
    std::string access_token) {
  access_token_fetcher_.reset();
  if (error.state() == GoogleServiceAuthError::NONE)
    OnAccessTokenRequestSucceeded(access_token);
  else
    OnAccessTokenRequestFailed(error);
}

void PerUserTopicRegistrationManager::OnAccessTokenRequestSucceeded(
    std::string access_token) {
  // Reset backoff time after successful response.
  request_access_token_backoff_.Reset();
  access_token_ = access_token;
  DoRegistrationUpdate();
}

void PerUserTopicRegistrationManager::OnAccessTokenRequestFailed(
    GoogleServiceAuthError error) {
  DCHECK_NE(error.state(), GoogleServiceAuthError::NONE);
  request_access_token_backoff_.InformOfRequest(false);
  request_access_token_retry_timer_.Start(
      FROM_HERE, request_access_token_backoff_.GetTimeUntilRelease(),
      base::BindRepeating(&PerUserTopicRegistrationManager::RequestAccessToken,
                          base::Unretained(this)));
}

}  // namespace syncer

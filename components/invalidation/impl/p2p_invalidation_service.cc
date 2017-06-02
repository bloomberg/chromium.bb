// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/p2p_invalidation_service.h"

#include <utility>

#include "base/command_line.h"
#include "components/invalidation/impl/invalidation_service_util.h"
#include "components/invalidation/impl/p2p_invalidator.h"
#include "google_apis/gaia/identity_provider.h"
#include "jingle/notifier/base/notifier_options.h"
#include "jingle/notifier/listener/push_client.h"
#include "net/url_request/url_request_context_getter.h"

namespace net {
class URLRequestContextGetter;
}

namespace invalidation {

P2PInvalidationService::P2PInvalidationService(
    std::unique_ptr<IdentityProvider> identity_provider,
    const scoped_refptr<net::URLRequestContextGetter>& request_context,
    syncer::P2PNotificationTarget notification_target)
    : identity_provider_(std::move(identity_provider)) {
  notifier::NotifierOptions notifier_options =
      ParseNotifierOptions(*base::CommandLine::ForCurrentProcess());
  notifier_options.request_context_getter = request_context;
  invalidator_id_ = GenerateInvalidatorClientId();
  invalidator_.reset(new syncer::P2PInvalidator(
          notifier::PushClient::CreateDefault(notifier_options),
          invalidator_id_,
          notification_target));
}

P2PInvalidationService::~P2PInvalidationService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void P2PInvalidationService::UpdateCredentials(const std::string& username,
                                               const std::string& password) {
  invalidator_->UpdateCredentials(username, password);
}

void P2PInvalidationService::RegisterInvalidationHandler(
    syncer::InvalidationHandler* handler) {
  invalidator_->RegisterHandler(handler);
}

bool P2PInvalidationService::UpdateRegisteredInvalidationIds(
    syncer::InvalidationHandler* handler,
    const syncer::ObjectIdSet& ids) {
  return invalidator_->UpdateRegisteredIds(handler, ids);
}

void P2PInvalidationService::UnregisterInvalidationHandler(
    syncer::InvalidationHandler* handler) {
  invalidator_->UnregisterHandler(handler);
}

void P2PInvalidationService::SendInvalidation(
    const syncer::ObjectIdSet& ids) {
  invalidator_->SendInvalidation(ids);
}

syncer::InvalidatorState P2PInvalidationService::GetInvalidatorState() const {
  return invalidator_->GetInvalidatorState();
}

std::string P2PInvalidationService::GetInvalidatorClientId() const {
  return invalidator_id_;
}

InvalidationLogger* P2PInvalidationService::GetInvalidationLogger() {
  return NULL;
}

void P2PInvalidationService::RequestDetailedStatus(
    base::Callback<void(const base::DictionaryValue&)> caller) const {
  base::DictionaryValue value;
  caller.Run(value);
}

IdentityProvider* P2PInvalidationService::GetIdentityProvider() {
  return identity_provider_.get();
}

}  // namespace invalidation

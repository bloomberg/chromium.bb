// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/invalidation/p2p_invalidation_service.h"

#include "base/command_line.h"
#include "chrome/browser/invalidation/invalidation_service_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "jingle/notifier/base/notifier_options.h"
#include "jingle/notifier/listener/push_client.h"
#include "sync/notifier/p2p_invalidator.h"

namespace net {
class URLRequestContextGetter;
}

namespace invalidation {

P2PInvalidationService::P2PInvalidationService(Profile* profile) {
  notifier::NotifierOptions notifier_options =
      ParseNotifierOptions(*CommandLine::ForCurrentProcess());
  notifier_options.request_context_getter = profile->GetRequestContext();
  invalidator_id_ = GenerateInvalidatorClientId();
  invalidator_.reset(new syncer::P2PInvalidator(
          notifier::PushClient::CreateDefault(notifier_options),
          invalidator_id_,
          syncer::NOTIFY_ALL));
}

P2PInvalidationService::~P2PInvalidationService() {
}

void P2PInvalidationService::UpdateCredentials(const std::string& username,
                                               const std::string& password) {
  invalidator_->UpdateCredentials(username, password);
}

void P2PInvalidationService::Shutdown() {
  invalidator_.reset();
}

void P2PInvalidationService::RegisterInvalidationHandler(
    syncer::InvalidationHandler* handler) {
  invalidator_->RegisterHandler(handler);
}

void P2PInvalidationService::UpdateRegisteredInvalidationIds(
    syncer::InvalidationHandler* handler,
    const syncer::ObjectIdSet& ids) {
  invalidator_->UpdateRegisteredIds(handler, ids);
}

void P2PInvalidationService::UnregisterInvalidationHandler(
    syncer::InvalidationHandler* handler) {
  invalidator_->UnregisterHandler(handler);
}

void P2PInvalidationService::AcknowledgeInvalidation(
    const invalidation::ObjectId& id,
    const syncer::AckHandle& ack_handle) {
  invalidator_->Acknowledge(id, ack_handle);
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

}  // namespace invalidation

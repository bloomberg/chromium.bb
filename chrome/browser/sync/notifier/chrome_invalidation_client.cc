// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/chrome_invalidation_client.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "chrome/browser/sync/notifier/cache_invalidation_packet_handler.h"
#include "chrome/browser/sync/notifier/invalidation_util.h"
#include "google/cacheinvalidation/invalidation-client-impl.h"

namespace sync_notifier {

ChromeInvalidationClient::ChromeInvalidationClient() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
}

ChromeInvalidationClient::~ChromeInvalidationClient() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  Stop();
}

void ChromeInvalidationClient::Start(
    const std::string& app_name,
    invalidation::InvalidationListener* listener,
    buzz::XmppClient* xmpp_client) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  Stop();

  chrome_system_resources_.StartScheduler();

  invalidation::ClientType client_type;
  client_type.set_type(invalidation::ClientType::CHROME_SYNC);
  invalidation::ClientConfig ticl_config;
  invalidation_client_.reset(
      new invalidation::InvalidationClientImpl(
          &chrome_system_resources_, client_type, app_name, listener,
          ticl_config));
  cache_invalidation_packet_handler_.reset(
      new CacheInvalidationPacketHandler(xmpp_client,
                                         invalidation_client_.get()));
}

void ChromeInvalidationClient::Stop() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  if (!invalidation_client_.get()) {
    DCHECK(!cache_invalidation_packet_handler_.get());
    return;
  }

  chrome_system_resources_.StopScheduler();

  cache_invalidation_packet_handler_.reset();
  invalidation_client_.reset();
}

void ChromeInvalidationClient::Register(
    const invalidation::ObjectId& oid,
    invalidation::RegistrationCallback* callback) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  invalidation_client_->Register(oid, callback);
}

void ChromeInvalidationClient::Unregister(
    const invalidation::ObjectId& oid,
    invalidation::RegistrationCallback* callback) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  invalidation_client_->Unregister(oid, callback);
}

}  // namespace sync_notifier

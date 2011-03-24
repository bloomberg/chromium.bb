// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/invalidation_notifier.h"

#include "base/logging.h"
#include "chrome/browser/sync/notifier/sync_notifier_observer.h"
#include "chrome/browser/sync/protocol/service_constants.h"
#include "chrome/browser/sync/syncable/model_type_payload_map.h"
#include "jingle/notifier/base/const_communicator.h"
#include "jingle/notifier/base/notifier_options_util.h"
#include "jingle/notifier/communicator/connection_options.h"
#include "net/base/host_port_pair.h"
#include "talk/xmpp/jid.h"
#include "talk/xmpp/xmppclientsettings.h"

namespace sync_notifier {

InvalidationNotifier::InvalidationNotifier(
    const notifier::NotifierOptions& notifier_options,
    net::HostResolver* host_resolver,
    net::CertVerifier* cert_verifier,
    const std::string& client_info)
    : state_(STOPPED),
      notifier_options_(notifier_options),
      host_resolver_(host_resolver),
      cert_verifier_(cert_verifier),
      client_info_(client_info) {
  DCHECK_EQ(notifier::NOTIFICATION_SERVER,
            notifier_options.notification_method);
}

InvalidationNotifier::~InvalidationNotifier() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
}

void InvalidationNotifier::AddObserver(SyncNotifierObserver* observer) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  observers_.AddObserver(observer);
}

void InvalidationNotifier::RemoveObserver(SyncNotifierObserver* observer) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  observers_.RemoveObserver(observer);
}

void InvalidationNotifier::SetState(const std::string& state) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  invalidation_state_ = state;
}

void InvalidationNotifier::UpdateCredentials(
    const std::string& email, const std::string& token) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  VLOG(1) << "Updating credentials for " << email;
  buzz::XmppClientSettings xmpp_client_settings =
      notifier::MakeXmppClientSettings(notifier_options_,
                                       email, token, SYNC_SERVICE_NAME);
  if (state_ >= CONNECTING) {
    login_->UpdateXmppSettings(xmpp_client_settings);
  } else {
    notifier::ConnectionOptions options;
    VLOG(1) << "First time updating credentials: connecting";
    login_.reset(
        new notifier::Login(this,
                            xmpp_client_settings,
                            notifier::ConnectionOptions(),
                            host_resolver_,
                            cert_verifier_,
                            notifier::GetServerList(notifier_options_),
                            notifier_options_.try_ssltcp_first,
                            notifier_options_.auth_mechanism));
    login_->StartConnection();
    state_ = CONNECTING;
  }
}

void InvalidationNotifier::UpdateEnabledTypes(
    const syncable::ModelTypeSet& types) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  enabled_types_ = types;
  if (state_ >= STARTED) {
    invalidation_client_.RegisterTypes(enabled_types_);
  }
  // If |invalidation_client_| hasn't been started yet, it will
  // register |enabled_types_| in OnConnect().
}

void InvalidationNotifier::SendNotification() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
}

void InvalidationNotifier::OnConnect(
    base::WeakPtr<talk_base::Task> base_task) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  VLOG(1) << "OnConnect";
  if (state_ >= STARTED) {
    invalidation_client_.ChangeBaseTask(base_task);
  } else {
    VLOG(1) << "First time connecting: starting invalidation client";
    // TODO(akalin): Make cache_guid() part of the client ID.  If we
    // do so and we somehow propagate it up to the server somehow, we
    // can make it so that we won't receive any notifications that
    // were generated from our own changes.
    const std::string kClientId = "invalidation_notifier";
    invalidation_client_.Start(
        kClientId, client_info_, invalidation_state_, this, this, base_task);
    invalidation_state_.clear();
    invalidation_client_.RegisterTypes(enabled_types_);
    state_ = STARTED;
  }
  FOR_EACH_OBSERVER(SyncNotifierObserver, observers_,
                    OnNotificationStateChange(true));
}

void InvalidationNotifier::OnDisconnect() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  FOR_EACH_OBSERVER(SyncNotifierObserver, observers_,
                    OnNotificationStateChange(false));
}

void InvalidationNotifier::OnInvalidate(
    syncable::ModelType model_type, const std::string& payload) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  DCHECK_GE(model_type, syncable::FIRST_REAL_MODEL_TYPE);
  DCHECK_LT(model_type, syncable::MODEL_TYPE_COUNT);
  VLOG(1) << "OnInvalidate: " << syncable::ModelTypeToString(model_type)
          << " " << payload;
  syncable::ModelTypeSet types;
  types.insert(model_type);
  EmitInvalidation(types, payload);
}

void InvalidationNotifier::OnInvalidateAll() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  VLOG(1) << "OnInvalidateAll";
  EmitInvalidation(enabled_types_, std::string());
}

void InvalidationNotifier::WriteState(const std::string& state) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  VLOG(1) << "WriteState";
  FOR_EACH_OBSERVER(SyncNotifierObserver, observers_, StoreState(state));
}

void InvalidationNotifier::EmitInvalidation(
    const syncable::ModelTypeSet& types, const std::string& payload) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  // TODO(akalin): Move all uses of ModelTypeBitSet for invalidations
  // to ModelTypeSet.
  syncable::ModelTypePayloadMap type_payloads =
      syncable::ModelTypePayloadMapFromBitSet(
          syncable::ModelTypeBitSetFromSet(types), payload);
  FOR_EACH_OBSERVER(SyncNotifierObserver, observers_,
                    OnIncomingNotification(type_payloads));
}

}  // namespace sync_notifier

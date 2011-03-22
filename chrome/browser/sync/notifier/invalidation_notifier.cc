// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/invalidation_notifier.h"

#include "chrome/browser/sync/notifier/sync_notifier_observer.h"
#include "chrome/browser/sync/protocol/service_constants.h"
#include "chrome/browser/sync/sessions/session_state.h"
#include "jingle/notifier/base/notifier_options_util.h"
#include "jingle/notifier/listener/notification_constants.h"

namespace sync_notifier {

InvalidationNotifier::InvalidationNotifier(
    const notifier::NotifierOptions& notifier_options,
    const std::string& client_info)
    : notifier_options_(notifier_options),
      server_notifier_thread_(notifier_options, client_info, this),
      logged_in_(false) {
  server_notifier_thread_.AddObserver(this);
  server_notifier_thread_.Start();
}

InvalidationNotifier::~InvalidationNotifier() {
  if (logged_in_) {
    server_notifier_thread_.Logout();
  }
  server_notifier_thread_.RemoveObserver(this);
}

void InvalidationNotifier::AddObserver(SyncNotifierObserver* observer) {
  observer_list_.AddObserver(observer);
}

void InvalidationNotifier::RemoveObserver(SyncNotifierObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void InvalidationNotifier::SetState(const std::string& state) {
  server_notifier_thread_.SetState(state);
}

void InvalidationNotifier::UpdateCredentials(
    const std::string& email, const std::string& token) {
  buzz::XmppClientSettings xmpp_client_settings =
      notifier::MakeXmppClientSettings(notifier_options_,
                                       email, token, SYNC_SERVICE_NAME);
  if (logged_in_) {
    server_notifier_thread_.UpdateXmppSettings(xmpp_client_settings);
  } else {
    server_notifier_thread_.Login(xmpp_client_settings);
    logged_in_ = true;
  }
}

void InvalidationNotifier::UpdateEnabledTypes(
    const syncable::ModelTypeSet& types) {
  server_notifier_thread_.UpdateEnabledTypes(types);
}

void InvalidationNotifier::SendNotification() {}

void InvalidationNotifier::WriteState(const std::string& state) {
  FOR_EACH_OBSERVER(SyncNotifierObserver, observer_list_,
      StoreState(state));
}

void InvalidationNotifier::OnConnectionStateChange(bool logged_in) {
  FOR_EACH_OBSERVER(SyncNotifierObserver, observer_list_,
                    OnNotificationStateChange(logged_in));
}

void InvalidationNotifier::OnSubscriptionStateChange(bool subscribed) {}

void InvalidationNotifier::OnIncomingNotification(
    const notifier::Notification& notification) {
  VLOG(1) << "Sync received server notification from "
          << notification.channel << ": " << notification.data;
  const std::string& model_type_list = notification.channel;
  const std::string& notification_payload = notification.data;

  syncable::ModelTypeBitSet model_types;
  if (!syncable::ModelTypeBitSetFromString(model_type_list, &model_types)) {
    LOG(DFATAL) << "Could not extract model types from server data.";
    model_types.set();
  }

  syncable::ModelTypePayloadMap type_payloads =
      syncable::ModelTypePayloadMapFromBitSet(
          model_types, notification_payload);
  FOR_EACH_OBSERVER(SyncNotifierObserver, observer_list_,
                    OnIncomingNotification(type_payloads));
}

void InvalidationNotifier::OnOutgoingNotification() {}

}  // namespace sync_notifier

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/invalidation_notifier.h"

#include "chrome/browser/sync/notifier/sync_notifier_observer.h"
#include "chrome/browser/sync/protocol/service_constants.h"
#include "chrome/browser/sync/sessions/session_state.h"
#include "jingle/notifier/listener/notification_constants.h"
#include "talk/xmpp/jid.h"
#include "talk/xmpp/xmppclientsettings.h"

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

namespace {

// TODO(akalin): Figure out a clean way to consolidate all
// XmppClientSettings code, e.g., the code in
// TalkMediatorImpl::SetAuthToken().
buzz::XmppClientSettings MakeXmppClientSettings(
    const std::string& email,
    const std::string& token,
    const notifier::NotifierOptions& notifier_options) {
  buzz::Jid jid = buzz::Jid(email);
  DCHECK(!jid.node().empty() && jid.IsValid());

  buzz::XmppClientSettings xmpp_client_settings;
  xmpp_client_settings.set_user(jid.node());
  xmpp_client_settings.set_resource("chrome-sync");
  xmpp_client_settings.set_host(jid.domain());
  xmpp_client_settings.set_use_tls(true);
  xmpp_client_settings.set_auth_cookie(
      notifier_options.invalidate_xmpp_login ?
      token + "bogus" : token);
  xmpp_client_settings.set_token_service(SYNC_SERVICE_NAME);
  if (notifier_options.allow_insecure_connection) {
    xmpp_client_settings.set_allow_plain(true);
    xmpp_client_settings.set_use_tls(false);
  }
  return xmpp_client_settings;
}

}  // namespace

void InvalidationNotifier::UpdateCredentials(
    const std::string& email, const std::string& token) {
  buzz::XmppClientSettings xmpp_client_settings =
      MakeXmppClientSettings(email, token, notifier_options_);
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

  browser_sync::sessions::TypePayloadMap type_payloads =
      browser_sync::sessions::MakeTypePayloadMapFromBitSet(
          model_types, notification_payload);
  FOR_EACH_OBSERVER(SyncNotifierObserver, observer_list_,
                    OnIncomingNotification(type_payloads));
}

void InvalidationNotifier::OnOutgoingNotification() {}

}  // namespace sync_notifier

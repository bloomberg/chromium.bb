// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/p2p_notifier.h"

#include "base/message_loop_proxy.h"
#include "chrome/browser/sync/notifier/sync_notifier_observer.h"
#include "chrome/browser/sync/protocol/service_constants.h"
#include "chrome/browser/sync/syncable/model_type_payload_map.h"
#include "jingle/notifier/listener/mediator_thread_impl.h"
#include "jingle/notifier/listener/talk_mediator_impl.h"

namespace sync_notifier {

namespace {
const char kSyncNotificationChannel[] = "http://www.google.com/chrome/sync";
const char kSyncNotificationData[] = "sync-ping-p2p";
}  // namespace

P2PNotifier::P2PNotifier(
    const notifier::NotifierOptions& notifier_options)
    : talk_mediator_(
        new notifier::TalkMediatorImpl(
            new notifier::MediatorThreadImpl(notifier_options),
            notifier_options)),
      logged_in_(false),
      notifications_enabled_(false),
      construction_message_loop_proxy_(
          base::MessageLoopProxy::CreateForCurrentThread()) {
  talk_mediator_->SetDelegate(this);
}

P2PNotifier::~P2PNotifier() {
  DCHECK(construction_message_loop_proxy_->BelongsToCurrentThread());
}

void P2PNotifier::AddObserver(SyncNotifierObserver* observer) {
  CheckOrSetValidThread();
  observer_list_.AddObserver(observer);
}

// Note: Since we need to shutdown TalkMediator on the method_thread, we are
// calling Logout on TalkMediator when the last observer is removed.
// Users will need to call UpdateCredentials again to use the same object.
// TODO(akalin): Think of a better solution to fix this.
void P2PNotifier::RemoveObserver(SyncNotifierObserver* observer) {
  CheckOrSetValidThread();
  observer_list_.RemoveObserver(observer);

  // Logout after the last observer is removed.
  if (observer_list_.size() == 0) {
   talk_mediator_->Logout();
  }
}

void P2PNotifier::SetState(const std::string& state) {
  CheckOrSetValidThread();
}

void P2PNotifier::UpdateCredentials(
    const std::string& email, const std::string& token) {
  CheckOrSetValidThread();
  // If already logged in, the new credentials will take effect on the
  // next reconnection.
  talk_mediator_->SetAuthToken(email, token, SYNC_SERVICE_NAME);
  if (!logged_in_) {
    if (!talk_mediator_->Login()) {
      LOG(DFATAL) << "Could not login for " << email;
      return;
    }

    notifier::Subscription subscription;
    subscription.channel = kSyncNotificationChannel;
    // There may be some subtle issues around case sensitivity of the
    // from field, but it doesn't matter too much since this is only
    // used in p2p mode (which is only used in testing).
    subscription.from = email;
    talk_mediator_->AddSubscription(subscription);

    logged_in_ = true;
  }
}

void P2PNotifier::UpdateEnabledTypes(const syncable::ModelTypeSet& types) {
  CheckOrSetValidThread();
  enabled_types_ = types;
  MaybeEmitNotification();
}

void P2PNotifier::SendNotification() {
  CheckOrSetValidThread();
  VLOG(1) << "Sending XMPP notification...";
  notifier::Notification notification;
  notification.channel = kSyncNotificationChannel;
  notification.data = kSyncNotificationData;
  talk_mediator_->SendNotification(notification);
}

void P2PNotifier::OnNotificationStateChange(bool notifications_enabled) {
  CheckOrSetValidThread();
  notifications_enabled_ = notifications_enabled;
  FOR_EACH_OBSERVER(SyncNotifierObserver, observer_list_,
      OnNotificationStateChange(notifications_enabled_));
  MaybeEmitNotification();
}

void P2PNotifier::OnIncomingNotification(
    const notifier::Notification& notification) {
  CheckOrSetValidThread();
  VLOG(1) << "Sync received P2P notification.";
  if (notification.channel != kSyncNotificationChannel) {
    LOG(WARNING) << "Notification from unexpected source: "
                 << notification.channel;
  }
  MaybeEmitNotification();
}

void P2PNotifier::OnOutgoingNotification() {}

void P2PNotifier::MaybeEmitNotification() {
  if (!logged_in_) {
    VLOG(1) << "Not logged in yet -- not emitting notification";
    return;
  }
  if (!notifications_enabled_) {
    VLOG(1) << "Notifications not enabled -- not emitting notification";
    return;
  }
  if (enabled_types_.empty()) {
    VLOG(1) << "No enabled types -- not emitting notification";
    return;
  }
  syncable::ModelTypePayloadMap type_payloads =
      syncable::ModelTypePayloadMapFromBitSet(
          syncable::ModelTypeBitSetFromSet(enabled_types_), std::string());
  FOR_EACH_OBSERVER(SyncNotifierObserver, observer_list_,
                    OnIncomingNotification(type_payloads));
}

void P2PNotifier::CheckOrSetValidThread() {
  if (method_message_loop_proxy_) {
    DCHECK(method_message_loop_proxy_->BelongsToCurrentThread());
  } else {
    method_message_loop_proxy_ =
        base::MessageLoopProxy::CreateForCurrentThread();
  }
}

}  // namespace sync_notifier

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/sync_notifier_impl.h"

#include "chrome/browser/sync/notifier/server_notifier_thread.h"
#include "chrome/browser/sync/notifier/sync_notifier_observer.h"
#include "chrome/browser/sync/protocol/service_constants.h"
#include "chrome/browser/sync/sessions/session_state.h"
#include "chrome/browser/sync/sync_constants.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "jingle/notifier/listener/mediator_thread_impl.h"
#include "jingle/notifier/listener/notification_constants.h"

// TODO(akalin): Split this class into two implementations - one for p2p and
// one for server issued notifications.
using notifier::TalkMediator;
using notifier::TalkMediatorImpl;
namespace sync_notifier {

SyncNotifierImpl::SyncNotifierImpl(
    const notifier::NotifierOptions& notifier_options)
    : notifier_options_(notifier_options),
      server_notifier_thread_(NULL) { }

SyncNotifierImpl::~SyncNotifierImpl() {
  scoped_ptr<TalkMediator> talk_mediator(talk_mediator_.release());

  // Shutdown the xmpp buzz connection.
  if (talk_mediator.get()) {
    VLOG(1) << "P2P: Mediator logout started.";
    talk_mediator->Logout();
    VLOG(1) << "P2P: Mediator logout completed.";
    talk_mediator.reset();

    // |server_notifier_thread_| is owned by |talk_mediator|. We NULL
    // it out here so as to not have a dangling pointer.
    server_notifier_thread_= NULL;
    VLOG(1) << "P2P: Mediator destroyed.";
  }
}

void SyncNotifierImpl::AddObserver(SyncNotifierObserver* observer) {
  observer_list_.AddObserver(observer);
}

void SyncNotifierImpl::RemoveObserver(SyncNotifierObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void SyncNotifierImpl::OnNotificationStateChange(bool notifications_enabled) {
  FOR_EACH_OBSERVER(SyncNotifierObserver, observer_list_,
      OnNotificationStateChange(notifications_enabled));

  // If using p2p notifications, generate a notification for all enabled types.
  // Used only for tests.
  if ((notifier_options_.notification_method !=
      notifier::NOTIFICATION_SERVER) && notifications_enabled) {
    browser_sync::sessions::TypePayloadMap model_types_with_payloads =
        browser_sync::sessions::MakeTypePayloadMapFromBitSet(
            syncable::ModelTypeBitSetFromSet(enabled_types_), std::string());
    FOR_EACH_OBSERVER(SyncNotifierObserver, observer_list_,
        OnIncomingNotification(model_types_with_payloads));
  }
}

void SyncNotifierImpl::OnIncomingNotification(
    const IncomingNotificationData& notification_data) {
  browser_sync::sessions::TypePayloadMap model_types_with_payloads;

  // Check if the service url is a sync URL.  An empty service URL is
  // treated as a legacy sync notification.  If we're listening to
  // server-issued notifications, no need to check the service_url.
  if (notifier_options_.notification_method ==
      notifier::NOTIFICATION_SERVER) {
    VLOG(1) << "Sync received server notification from " <<
        notification_data.service_url << ": " <<
        notification_data.service_specific_data;
    syncable::ModelTypeBitSet model_types;
    const std::string& model_type_list = notification_data.service_url;
    const std::string& notification_payload =
        notification_data.service_specific_data;

    if (!syncable::ModelTypeBitSetFromString(model_type_list, &model_types)) {
      LOG(DFATAL) << "Could not extract model types from server data.";
      model_types.set();
    }

    model_types_with_payloads =
        browser_sync::sessions::MakeTypePayloadMapFromBitSet(model_types,
            notification_payload);
  } else if (notification_data.service_url.empty() ||
             (notification_data.service_url ==
              browser_sync::kSyncLegacyServiceUrl) ||
             (notification_data.service_url ==
              browser_sync::kSyncServiceUrl)) {
    VLOG(1) << "Sync received P2P notification.";

    // Catch for sync integration tests (uses p2p). Just set all enabled
    // datatypes.
    model_types_with_payloads =
        browser_sync::sessions::MakeTypePayloadMapFromBitSet(
            syncable::ModelTypeBitSetFromSet(enabled_types_), std::string());
  } else {
    LOG(WARNING) << "Notification fron unexpected source: "
                 << notification_data.service_url;
  }

  FOR_EACH_OBSERVER(SyncNotifierObserver, observer_list_,
      OnIncomingNotification(model_types_with_payloads));
}

void SyncNotifierImpl::WriteState(const std::string& state) {
  FOR_EACH_OBSERVER(SyncNotifierObserver, observer_list_,
      StoreState(state));
}

void SyncNotifierImpl::UpdateCredentials(
    const std::string& email, const std::string& token) {
  // Reset talk_mediator_ before creating ServerNotifierThread/MediatorThread,
  // to avoid any problems with having two of those threads at the same time.
  talk_mediator_.reset();

  if (notifier_options_.notification_method ==
      notifier::NOTIFICATION_SERVER) {
    // |talk_mediator_| takes ownership of |sync_notifier_thread_|
    // but it is guaranteed that |sync_notifier_thread_| is destroyed only
    // when |talk_mediator_| is (see the comments in talk_mediator.h).
    server_notifier_thread_ = new sync_notifier::ServerNotifierThread(
        notifier_options_, state_, this);
    talk_mediator_.reset(
        new TalkMediatorImpl(server_notifier_thread_,
                             notifier_options_.invalidate_xmpp_login,
                             notifier_options_.allow_insecure_connection));

    // Since we may be initialized more than once, make sure that any
    // newly created server notifier thread has the latest enabled types.
    server_notifier_thread_->UpdateEnabledTypes(enabled_types_);
  } else {
    notifier::MediatorThread* mediator_thread =
        new notifier::MediatorThreadImpl(notifier_options_);
    talk_mediator_.reset(
        new TalkMediatorImpl(mediator_thread,
                             notifier_options_.invalidate_xmpp_login,
                             notifier_options_.allow_insecure_connection));
    talk_mediator_->AddSubscribedServiceUrl(browser_sync::kSyncServiceUrl);
    server_notifier_thread_ = NULL;
  }
  talk_mediator_->SetDelegate(this);
  talk_mediator_->SetAuthToken(email, token, SYNC_SERVICE_NAME);
  talk_mediator_->Login();
}

void SyncNotifierImpl::SetState(const std::string& state) {
  state_ = state;
}

void SyncNotifierImpl::UpdateEnabledTypes(const syncable::ModelTypeSet& types) {
  enabled_types_ = types;
  if (server_notifier_thread_ != NULL) {
    server_notifier_thread_->UpdateEnabledTypes(types);
  }
}

void SyncNotifierImpl::SendNotification() {
  // Do nothing if we are using server based notifications.
  if (notifier_options_.notification_method ==
      notifier::NOTIFICATION_SERVER) {
    return;
  }

  if (!talk_mediator_.get()) {
    NOTREACHED() << "Cannot send notification: talk_mediator_ is NULL";
    return;
  }

  VLOG(1) << "Sending XMPP notification...";
  OutgoingNotificationData notification_data;
  notification_data.service_id = browser_sync::kSyncServiceId;
  notification_data.service_url = browser_sync::kSyncServiceUrl;
  notification_data.send_content = true;
  notification_data.priority = browser_sync::kSyncPriority;
  notification_data.write_to_cache_only = true;
  notification_data.service_specific_data =
      browser_sync::kSyncServiceSpecificData;
  notification_data.require_subscription = true;
  bool success = talk_mediator_->SendNotification(notification_data);
  if (success) {
    VLOG(1) << "Sent XMPP notification";
  } else {
    VLOG(1) << "Could not send XMPP notification";
  }
}
}  // namespace sync_notifier

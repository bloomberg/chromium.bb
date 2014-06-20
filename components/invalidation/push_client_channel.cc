// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/push_client_channel.h"

#include "base/stl_util.h"
#include "components/invalidation/notifier_reason_util.h"
#include "google/cacheinvalidation/client_gateway.pb.h"
#include "google/cacheinvalidation/types.pb.h"
#include "jingle/notifier/listener/push_client.h"

namespace syncer {

namespace {

const char kBotJid[] = "tango@bot.talk.google.com";
const char kChannelName[] = "tango_raw";

}  // namespace

PushClientChannel::PushClientChannel(
    scoped_ptr<notifier::PushClient> push_client)
    : push_client_(push_client.Pass()),
      scheduling_hash_(0),
      sent_messages_count_(0) {
  push_client_->AddObserver(this);
  notifier::Subscription subscription;
  subscription.channel = kChannelName;
  subscription.from = "";
  notifier::SubscriptionList subscriptions;
  subscriptions.push_back(subscription);
  push_client_->UpdateSubscriptions(subscriptions);
}

PushClientChannel::~PushClientChannel() {
  push_client_->RemoveObserver(this);
}

void PushClientChannel::UpdateCredentials(
    const std::string& email, const std::string& token) {
  push_client_->UpdateCredentials(email, token);
}

int PushClientChannel::GetInvalidationClientType() {
#if defined(OS_IOS)
  return ipc::invalidation::ClientType::CHROME_SYNC_IOS;
#else
  return ipc::invalidation::ClientType::CHROME_SYNC;
#endif
}

void PushClientChannel::RequestDetailedStatus(
    base::Callback<void(const base::DictionaryValue&)> callback) {
  callback.Run(*CollectDebugData());
}

void PushClientChannel::SendMessage(const std::string& message) {
  std::string encoded_message;
  EncodeMessage(&encoded_message, message, service_context_, scheduling_hash_);

  notifier::Recipient recipient;
  recipient.to = kBotJid;
  notifier::Notification notification;
  notification.channel = kChannelName;
  notification.recipients.push_back(recipient);
  notification.data = encoded_message;
  push_client_->SendNotification(notification);
  sent_messages_count_++;
}

void PushClientChannel::OnNotificationsEnabled() {
  NotifyNetworkStatusChange(true);
  NotifyChannelStateChange(INVALIDATIONS_ENABLED);
}

void PushClientChannel::OnNotificationsDisabled(
    notifier::NotificationsDisabledReason reason) {
  NotifyNetworkStatusChange(false);
  NotifyChannelStateChange(FromNotifierReason(reason));
}

void PushClientChannel::OnIncomingNotification(
    const notifier::Notification& notification) {
  std::string message;
  std::string service_context;
  int64 scheduling_hash;
  if (!DecodeMessage(
           notification.data, &message, &service_context, &scheduling_hash)) {
    DLOG(ERROR) << "Could not parse ClientGatewayMessage";
    return;
  }
  if (DeliverIncomingMessage(message)) {
    service_context_ = service_context;
    scheduling_hash_ = scheduling_hash;
  }
}

const std::string& PushClientChannel::GetServiceContextForTest() const {
  return service_context_;
}

int64 PushClientChannel::GetSchedulingHashForTest() const {
  return scheduling_hash_;
}

std::string PushClientChannel::EncodeMessageForTest(
    const std::string& message,
    const std::string& service_context,
    int64 scheduling_hash) {
  std::string encoded_message;
  EncodeMessage(&encoded_message, message, service_context, scheduling_hash);
  return encoded_message;
}

bool PushClientChannel::DecodeMessageForTest(const std::string& data,
                                             std::string* message,
                                             std::string* service_context,
                                             int64* scheduling_hash) {
  return DecodeMessage(data, message, service_context, scheduling_hash);
}

void PushClientChannel::EncodeMessage(std::string* encoded_message,
                                      const std::string& message,
                                      const std::string& service_context,
                                      int64 scheduling_hash) {
  ipc::invalidation::ClientGatewayMessage envelope;
  envelope.set_is_client_to_server(true);
  if (!service_context.empty()) {
    envelope.set_service_context(service_context);
    envelope.set_rpc_scheduling_hash(scheduling_hash);
  }
  envelope.set_network_message(message);
  envelope.SerializeToString(encoded_message);
}

bool PushClientChannel::DecodeMessage(const std::string& data,
                                      std::string* message,
                                      std::string* service_context,
                                      int64* scheduling_hash) {
  ipc::invalidation::ClientGatewayMessage envelope;
  if (!envelope.ParseFromString(data)) {
    return false;
  }
  *message = envelope.network_message();
  if (envelope.has_service_context()) {
    *service_context = envelope.service_context();
  }
  if (envelope.has_rpc_scheduling_hash()) {
    *scheduling_hash = envelope.rpc_scheduling_hash();
  }
  return true;
}

scoped_ptr<base::DictionaryValue> PushClientChannel::CollectDebugData() const {
  scoped_ptr<base::DictionaryValue> status(new base::DictionaryValue);
  status->SetString("PushClientChannel.NetworkChannel", "Push Client");
  status->SetInteger("PushClientChannel.SentMessages", sent_messages_count_);
  status->SetInteger("PushClientChannel.ReceivedMessages",
                     SyncNetworkChannel::GetReceivedMessagesCount());
  return status.Pass();
}

}  // namespace syncer

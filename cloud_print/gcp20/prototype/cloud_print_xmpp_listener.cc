// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/gcp20/prototype/cloud_print_xmpp_listener.h"

#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "cloud_print/gcp20/prototype/cloud_print_url_request_context_getter.h"
#include "jingle/notifier/base/notifier_options.h"
#include "jingle/notifier/listener/push_client.h"

namespace {

const char kCloudPrintPushNotificationsSource[] = "cloudprint.google.com";

// Splits message into two parts (e.g. '<device_id>/delete' will be splitted
// into '<device_id>' and '/delete').
void TokenizeXmppMessage(const std::string& message, std::string* device_id,
                         std::string* path) {
  std::string::size_type pos = message.find('/');
  if (pos != std::string::npos) {
    *device_id = message.substr(0, pos);
    *path = message.substr(pos);
  } else {
    *device_id = message;
    path->clear();
  }
}

}  // namespace

CloudPrintXmppListener::CloudPrintXmppListener(
    const std::string& robot_email,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    Delegate* delegate)
    : robot_email_(robot_email),
      context_getter_(new CloudPrintURLRequestContextGetter(task_runner)),
      delegate_(delegate) {
}

CloudPrintXmppListener::~CloudPrintXmppListener() {
  push_client_->RemoveObserver(this);
  push_client_.reset();
}

void CloudPrintXmppListener::Connect(const std::string& access_token) {
  push_client_.reset();
  access_token_ = access_token;

  notifier::NotifierOptions options;
  options.request_context_getter = context_getter_;
  options.auth_mechanism = "X-OAUTH2";
  options.try_ssltcp_first = true;
  push_client_ = notifier::PushClient::CreateDefault(options);

  notifier::Subscription subscription;
  subscription.channel = kCloudPrintPushNotificationsSource;
  subscription.from = kCloudPrintPushNotificationsSource;

  notifier::SubscriptionList list;
  list.push_back(subscription);

  push_client_->UpdateSubscriptions(list);
  push_client_->AddObserver(this);
  push_client_->UpdateCredentials(robot_email_, access_token_);
}

void CloudPrintXmppListener::OnNotificationsEnabled() {
  delegate_->OnXmppConnected();
}

void CloudPrintXmppListener::OnNotificationsDisabled(
    notifier::NotificationsDisabledReason reason) {
  switch (reason) {
    case notifier::NOTIFICATION_CREDENTIALS_REJECTED:
      delegate_->OnXmppAuthError();
      break;
    case notifier::TRANSIENT_NOTIFICATION_ERROR:
      delegate_->OnXmppNetworkError();
      break;
    default:
      NOTREACHED() << "XMPP failed with unexpected reason code: " << reason;
  }
}

void CloudPrintXmppListener::OnIncomingNotification(
    const notifier::Notification& notification) {
  std::string device_id;
  std::string path;
  TokenizeXmppMessage(notification.data, &device_id, &path);

  if (path.empty() || path == "/") {
    delegate_->OnXmppNewPrintJob(device_id);
  } else if (path == "/update_settings") {
    delegate_->OnXmppNewLocalSettings(device_id);
  } else if (path == "/delete") {
    delegate_->OnXmppDeleteNotification(device_id);
  } else {
    LOG(ERROR) << "Cannot parse XMPP notification: " << notification.data;
  }
}

void CloudPrintXmppListener::OnPingResponse() {
  // TODO(maksymb): Implement pings
  NOTIMPLEMENTED();
}


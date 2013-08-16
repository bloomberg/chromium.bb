// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/gcp20/prototype/cloud_print_xmpp_listener.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "cloud_print/gcp20/prototype/cloud_print_url_request_context_getter.h"
#include "jingle/notifier/base/notifier_options.h"
#include "jingle/notifier/listener/push_client.h"

namespace {

const int kUrgentPingInterval = 60;  // in seconds
const int kPingTimeout = 30;  // in seconds
const double kRandomDelayPercentage = 0.2;

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
    int standard_ping_interval,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    Delegate* delegate)
    : robot_email_(robot_email),
      standard_ping_interval_(standard_ping_interval),
      ping_timeouts_posted_(0),
      ping_responses_pending_(0),
      ping_scheduled_(false),
      context_getter_(new CloudPrintURLRequestContextGetter(task_runner)),
      delegate_(delegate) {
}

CloudPrintXmppListener::~CloudPrintXmppListener() {
  if (push_client_) {
    push_client_->RemoveObserver(this);
    push_client_.reset();
  }
}

void CloudPrintXmppListener::Connect(const std::string& access_token) {
  access_token_ = access_token;
  ping_responses_pending_ = 0;
  ping_scheduled_ = false;

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

void CloudPrintXmppListener::set_ping_interval(int interval) {
  standard_ping_interval_ = interval;
}

void CloudPrintXmppListener::OnNotificationsEnabled() {
  delegate_->OnXmppConnected();
  SchedulePing();
}

void CloudPrintXmppListener::OnNotificationsDisabled(
    notifier::NotificationsDisabledReason reason) {
  switch (reason) {
    case notifier::NOTIFICATION_CREDENTIALS_REJECTED:
      delegate_->OnXmppAuthError();
      break;
    case notifier::TRANSIENT_NOTIFICATION_ERROR:
      Disconnect();
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
  ping_responses_pending_ = 0;
  SchedulePing();
}

void CloudPrintXmppListener::Disconnect() {
  push_client_.reset();
  delegate_->OnXmppNetworkError();
}

void CloudPrintXmppListener::SchedulePing() {
  if (ping_scheduled_)
    return;

  DCHECK_LE(kPingTimeout, kUrgentPingInterval);
  int delay = (ping_responses_pending_ > 0)
      ? kUrgentPingInterval - kPingTimeout
      : standard_ping_interval_;

  delay += base::RandInt(0, delay*kRandomDelayPercentage);

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&CloudPrintXmppListener::SendPing, AsWeakPtr()),
      base::TimeDelta::FromSeconds(delay));

  ping_scheduled_ = true;
}

void CloudPrintXmppListener::SendPing() {
  ping_scheduled_ = false;

  DCHECK(push_client_);
  push_client_->SendPing();
  ++ping_responses_pending_;

  base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&CloudPrintXmppListener::OnPingTimeoutReached, AsWeakPtr()),
        base::TimeDelta::FromSeconds(kPingTimeout));
  ++ping_timeouts_posted_;
}

void CloudPrintXmppListener::OnPingTimeoutReached() {
  DCHECK_GT(ping_timeouts_posted_, 0);
  --ping_timeouts_posted_;
  if (ping_timeouts_posted_ > 0)
    return;  // Fake (old) timeout.

  switch (ping_responses_pending_) {
    case 0:
      break;
    case 1:
      SchedulePing();
      break;
    case 2:
      Disconnect();
      break;
    default:
      NOTREACHED();
  }
}


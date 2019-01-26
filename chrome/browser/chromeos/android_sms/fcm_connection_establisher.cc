// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/android_sms/fcm_connection_establisher.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"

namespace chromeos {

namespace android_sms {

const int FcmConnectionEstablisher::kMaxRetryCount = 7;

constexpr base::TimeDelta FcmConnectionEstablisher::kRetryDelay =
    base::TimeDelta::FromSeconds(5);

// Start message is sent when establishing a connection for the first time on
// log-in. This allows the service worker to freshly subscribe to push
// notifications.
const char FcmConnectionEstablisher::kStartFcmMessage[] =
    "start_fcm_connection";

// Resume message is sent to notify the service worker to resume handling
// background notifications after all "Messages for Web" web pages have
// been closed.
const char FcmConnectionEstablisher::kResumeFcmMessage[] =
    "resume_fcm_connection";

// Stop message is sent to notify the service worker to unsubscribe from
// push messages when the messages feature is disabled.
const char FcmConnectionEstablisher::kStopFcmMessage[] = "stop_fcm_connection";

FcmConnectionEstablisher::PendingServiceWorkerMessage::
    PendingServiceWorkerMessage(
        GURL service_worker_scope,
        std::string message_content,
        content::ServiceWorkerContext* service_worker_context)
    : service_worker_scope(service_worker_scope),
      message_content(message_content),
      service_worker_context(service_worker_context) {}

FcmConnectionEstablisher::InFlightMessage::InFlightMessage(
    PendingServiceWorkerMessage message)
    : message(message) {}

FcmConnectionEstablisher::FcmConnectionEstablisher(
    std::unique_ptr<base::OneShotTimer> retry_timer)
    : retry_timer_(std::move(retry_timer)), weak_ptr_factory_(this) {}
FcmConnectionEstablisher::~FcmConnectionEstablisher() = default;

void FcmConnectionEstablisher::EstablishConnection(
    const GURL& url,
    ConnectionMode connection_mode,
    content::ServiceWorkerContext* service_worker_context) {
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(
          &FcmConnectionEstablisher::SendMessageToServiceWorkerWithRetries,
          weak_ptr_factory_.GetWeakPtr(), url,
          GetMessageForConnectionMode(connection_mode),
          service_worker_context));
}

void FcmConnectionEstablisher::TearDownConnection(
    const GURL& url,
    content::ServiceWorkerContext* service_worker_context) {
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(
          &FcmConnectionEstablisher::SendMessageToServiceWorkerWithRetries,
          weak_ptr_factory_.GetWeakPtr(), url, kStopFcmMessage,
          service_worker_context));
}

// static
std::string FcmConnectionEstablisher::GetMessageForConnectionMode(
    ConnectionMode connection_mode) {
  switch (connection_mode) {
    case ConnectionMode::kStartConnection:
      return kStartFcmMessage;
    case ConnectionMode::kResumeExistingConnection:
      return kResumeFcmMessage;
  }
  NOTREACHED();
  return "";
}

void FcmConnectionEstablisher::SendMessageToServiceWorkerWithRetries(
    const GURL& url,
    std::string message_string,
    content::ServiceWorkerContext* service_worker_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  message_queue_.emplace(url, message_string, service_worker_context);
  ProcessMessageQueue();
}

void FcmConnectionEstablisher::ProcessMessageQueue() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (in_flight_message_)
    return;

  if (message_queue_.empty())
    return;

  in_flight_message_.emplace(message_queue_.front());
  message_queue_.pop();
  SendInFlightMessage();
}

void FcmConnectionEstablisher::SendInFlightMessage() {
  const PendingServiceWorkerMessage& message = in_flight_message_->message;
  blink::TransferableMessage msg;
  msg.owned_encoded_message =
      blink::EncodeStringMessage(base::UTF8ToUTF16(message.message_content));
  msg.encoded_message = msg.owned_encoded_message;

  PA_LOG(VERBOSE) << "Dispatching message " << message.message_content;
  message.service_worker_context->StartServiceWorkerAndDispatchMessage(
      message.service_worker_scope, std::move(msg),
      base::BindOnce(&FcmConnectionEstablisher::OnMessageDispatchResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FcmConnectionEstablisher::OnMessageDispatchResult(bool status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(in_flight_message_);
  PA_LOG(VERBOSE) << "Service worker message returned status: " << status;

  if (!status && in_flight_message_->retry_count < kMaxRetryCount) {
    base::TimeDelta retry_delay =
        kRetryDelay * (1 << in_flight_message_->retry_count);
    in_flight_message_->retry_count++;
    PA_LOG(VERBOSE) << "Scheduling retry with delay " << retry_delay;
    retry_timer_->Start(
        FROM_HERE, retry_delay,
        base::BindOnce(&FcmConnectionEstablisher::SendInFlightMessage,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  if (in_flight_message_->retry_count >= kMaxRetryCount) {
    PA_LOG(WARNING) << "Max retries attempted when dispatching message "
                    << in_flight_message_->message.message_content;
  }

  in_flight_message_.reset();
  ProcessMessageQueue();
}

}  // namespace android_sms

}  // namespace chromeos

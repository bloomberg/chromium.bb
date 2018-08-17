// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/android_sms/connection_establisher_impl.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/android_sms/android_sms_urls.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/message_port/string_message_codec.h"

namespace chromeos {

namespace android_sms {

namespace {
const char kStartStreamingMessageString[] = "start_streaming_connection";
}

ConnectionEstablisherImpl::ConnectionEstablisherImpl() = default;
ConnectionEstablisherImpl::~ConnectionEstablisherImpl() = default;

void ConnectionEstablisherImpl::EstablishConnection(
    content::ServiceWorkerContext* service_worker_context) {
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(&ConnectionEstablisherImpl::SendStartStreamingMessage,
                     base::Unretained(this), service_worker_context));
}

void ConnectionEstablisherImpl::SendStartStreamingMessage(
    content::ServiceWorkerContext* service_worker_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (is_connected_)
    return;

  blink::TransferableMessage msg;
  msg.owned_encoded_message = blink::EncodeStringMessage(
      base::UTF8ToUTF16(kStartStreamingMessageString));
  msg.encoded_message = msg.owned_encoded_message;

  is_connected_ = true;
  service_worker_context->StartServiceWorkerAndDispatchLongRunningMessage(
      GetAndroidMessagesURL(), std::move(msg),
      base::BindOnce(&ConnectionEstablisherImpl::OnMessageDispatchResult,
                     base::Unretained(this)));
}

void ConnectionEstablisherImpl::OnMessageDispatchResult(bool status) {
  // When message dispatch result callback is called, it means that the service
  // worker resolved it's message handler promise and is not holding a
  // background connection.
  is_connected_ = false;
}

}  // namespace android_sms

}  // namespace chromeos

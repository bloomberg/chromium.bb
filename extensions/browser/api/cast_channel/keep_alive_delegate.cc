// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/cast_channel/keep_alive_delegate.h"

#include <string>
#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "extensions/browser/api/cast_channel/cast_socket.h"
#include "extensions/browser/api/cast_channel/logger.h"
#include "extensions/common/api/cast_channel/cast_channel.pb.h"
#include "extensions/common/api/cast_channel/logging.pb.h"
#include "net/base/net_errors.h"

namespace extensions {
namespace api {
namespace cast_channel {
namespace {

const char kHeartbeatNamespace[] = "urn:x-cast:com.google.cast.tp.heartbeat";
const char kPingSenderId[] = "chrome";
const char kPingReceiverId[] = "receiver-0";
const char kTypeNodeId[] = "type";

// Parses the JSON-encoded payload of |message| and returns the value in the
// "type" field or the empty string if the parse fails or the field is not
// found.
std::string ParseForPayloadType(const CastMessage& message) {
  std::unique_ptr<base::Value> parsed_payload(
      base::JSONReader::Read(message.payload_utf8()));
  base::DictionaryValue* payload_as_dict;
  if (!parsed_payload || !parsed_payload->GetAsDictionary(&payload_as_dict))
    return std::string();
  std::string type_string;
  if (!payload_as_dict->GetString(kTypeNodeId, &type_string))
    return std::string();
  return type_string;
}

}  // namespace

// static
const char KeepAliveDelegate::kHeartbeatPingType[] = "PING";

// static
const char KeepAliveDelegate::kHeartbeatPongType[] = "PONG";

// static
CastMessage KeepAliveDelegate::CreateKeepAliveMessage(
    const char* message_type) {
  CastMessage output;
  output.set_protocol_version(CastMessage::CASTV2_1_0);
  output.set_source_id(kPingSenderId);
  output.set_destination_id(kPingReceiverId);
  output.set_namespace_(kHeartbeatNamespace);
  base::DictionaryValue type_dict;
  type_dict.SetString(kTypeNodeId, message_type);
  if (!base::JSONWriter::Write(type_dict, output.mutable_payload_utf8())) {
    LOG(ERROR) << "Failed to serialize dictionary.";
    return output;
  }
  output.set_payload_type(
      CastMessage::PayloadType::CastMessage_PayloadType_STRING);
  return output;
}

KeepAliveDelegate::KeepAliveDelegate(
    CastSocket* socket,
    scoped_refptr<Logger> logger,
    std::unique_ptr<CastTransport::Delegate> inner_delegate,
    base::TimeDelta ping_interval,
    base::TimeDelta liveness_timeout)
    : started_(false),
      socket_(socket),
      logger_(logger),
      inner_delegate_(std::move(inner_delegate)),
      liveness_timeout_(liveness_timeout),
      ping_interval_(ping_interval) {
  DCHECK(ping_interval_ < liveness_timeout_);
  DCHECK(inner_delegate_);
  DCHECK(socket_);
  ping_message_ = CreateKeepAliveMessage(kHeartbeatPingType);
  pong_message_ = CreateKeepAliveMessage(kHeartbeatPongType);
}

KeepAliveDelegate::~KeepAliveDelegate() {
}

void KeepAliveDelegate::SetTimersForTest(
    std::unique_ptr<base::Timer> injected_ping_timer,
    std::unique_ptr<base::Timer> injected_liveness_timer) {
  ping_timer_ = std::move(injected_ping_timer);
  liveness_timer_ = std::move(injected_liveness_timer);
}

void KeepAliveDelegate::Start() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!started_);

  VLOG(1) << "Starting keep-alive timers.";
  VLOG(1) << "Ping timeout: " << ping_interval_;
  VLOG(1) << "Liveness timeout: " << liveness_timeout_;

  // Use injected mock timers, if provided.
  if (!ping_timer_) {
    ping_timer_.reset(new base::Timer(true, false));
  }
  if (!liveness_timer_) {
    liveness_timer_.reset(new base::Timer(true, false));
  }

  ping_timer_->Start(
      FROM_HERE, ping_interval_,
      base::Bind(&KeepAliveDelegate::SendKeepAliveMessage,
                 base::Unretained(this), ping_message_, kHeartbeatPingType));
  liveness_timer_->Start(
      FROM_HERE, liveness_timeout_,
      base::Bind(&KeepAliveDelegate::LivenessTimeout, base::Unretained(this)));

  started_ = true;
  inner_delegate_->Start();
}

void KeepAliveDelegate::ResetTimers() {
  DCHECK(started_);
  ping_timer_->Reset();
  liveness_timer_->Reset();
}

void KeepAliveDelegate::SendKeepAliveMessage(const CastMessage& message,
                                             const char* message_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  VLOG(2) << "Sending " << message_type;
  socket_->transport()->SendMessage(
      message, base::Bind(&KeepAliveDelegate::SendKeepAliveMessageComplete,
                          base::Unretained(this), message_type));
}

void KeepAliveDelegate::SendKeepAliveMessageComplete(const char* message_type,
                                                     int rv) {
  VLOG(2) << "Sending " << message_type << " complete, rv=" << rv;
  if (rv != net::OK) {
    // An error occurred while sending the ping response.
    VLOG(1) << "Error sending " << message_type;
    logger_->LogSocketEventWithRv(socket_->id(), proto::PING_WRITE_ERROR, rv);
    OnError(cast_channel::CHANNEL_ERROR_SOCKET_ERROR);
  }
}

void KeepAliveDelegate::LivenessTimeout() {
  OnError(cast_channel::CHANNEL_ERROR_PING_TIMEOUT);
  Stop();
}

// CastTransport::Delegate interface.
void KeepAliveDelegate::OnError(ChannelError error_state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  VLOG(1) << "KeepAlive::OnError: " << error_state;
  inner_delegate_->OnError(error_state);
  Stop();
}

void KeepAliveDelegate::OnMessage(const CastMessage& message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  VLOG(2) << "KeepAlive::OnMessage : " << message.payload_utf8();

  if (started_)
    ResetTimers();

  // PING and PONG messages are intercepted and handled by KeepAliveDelegate
  // here. All other messages are passed through to |inner_delegate_|.
  const std::string payload_type = ParseForPayloadType(message);
  if (payload_type == kHeartbeatPingType) {
    VLOG(2) << "Received PING.";
    if (started_)
      SendKeepAliveMessage(pong_message_, kHeartbeatPongType);
  } else if (payload_type == kHeartbeatPongType) {
    VLOG(2) << "Received PONG.";
  } else {
    inner_delegate_->OnMessage(message);
  }
}

void KeepAliveDelegate::Stop() {
  if (started_) {
    started_ = false;
    ping_timer_->Stop();
    liveness_timer_->Stop();
  }
}

}  // namespace cast_channel
}  // namespace api
}  // namespace extensions

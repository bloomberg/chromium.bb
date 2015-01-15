// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "extensions/browser/api/cast_channel/cast_message_util.h"
#include "extensions/browser/api/cast_channel/cast_socket.h"
#include "extensions/browser/api/cast_channel/keep_alive_delegate.h"
#include "extensions/common/api/cast_channel/cast_channel.pb.h"
#include "extensions/common/api/cast_channel/logging.pb.h"
#include "net/base/net_errors.h"

namespace extensions {
namespace core_api {
namespace cast_channel {
namespace {

const char kHeartbeatNamespace[] = "urn:x-cast:com.google.cast.tp.heartbeat";
const char kPingSenderId[] = "chrome";
const char kPingReceiverId[] = "receiver-0";
const char kTypeNodeId[] = "type";

// Determines if the JSON-encoded payload is equivalent to
// { "type": |chk_type| }
bool NestedPayloadTypeEquals(const std::string& chk_type,
                             const CastMessage& message) {
  MessageInfo message_info;
  CastMessageToMessageInfo(message, &message_info);
  std::string type_json;
  if (!message_info.data->GetAsString(&type_json)) {
    return false;
  }
  scoped_ptr<base::Value> type_value(base::JSONReader::Read(type_json));
  if (!type_value.get()) {
    return false;
  }

  base::DictionaryValue* type_dict;
  if (!type_value->GetAsDictionary(&type_dict)) {
    return false;
  }

  std::string type_string;
  return (type_dict->HasKey(kTypeNodeId) &&
          type_dict->GetString(kTypeNodeId, &type_string) &&
          type_string == chk_type);
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
  if (!base::JSONWriter::Write(&type_dict, output.mutable_payload_utf8())) {
    LOG(ERROR) << "Failed to serialize dictionary.";
    return output;
  }
  output.set_payload_type(
      CastMessage::PayloadType::CastMessage_PayloadType_STRING);
  return output;
}

KeepAliveDelegate::KeepAliveDelegate(
    CastSocket* socket,
    scoped_ptr<CastTransport::Delegate> inner_delegate,
    base::TimeDelta ping_interval,
    base::TimeDelta liveness_timeout)
    : started_(false),
      socket_(socket),
      inner_delegate_(inner_delegate.Pass()),
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
    scoped_ptr<base::Timer> injected_ping_timer,
    scoped_ptr<base::Timer> injected_liveness_timer) {
  ping_timer_ = injected_ping_timer.Pass();
  liveness_timer_ = injected_liveness_timer.Pass();
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
                                             const char* message_type) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  VLOG(1) << "Sending " << message_type;
  socket_->transport()->SendMessage(
      message, base::Bind(&KeepAliveDelegate::SendKeepAliveMessageComplete,
                          base::Unretained(this), message_type));
}

void KeepAliveDelegate::SendKeepAliveMessageComplete(const char* message_type,
                                                     int rv) const {
  VLOG(2) << "Sending " << message_type << " complete, rv=" << rv;
  if (rv != net::OK) {
    // An error occurred while sending the ping response.
    // Close the connection.
    VLOG(1) << "Error sending " << message_type;
    inner_delegate_->OnError(cast_channel::CHANNEL_ERROR_SOCKET_ERROR,
                             LastErrors());
  }
}

void KeepAliveDelegate::LivenessTimeout() const {
  VLOG(1) << "Ping timeout";
  inner_delegate_->OnError(cast_channel::CHANNEL_ERROR_PING_TIMEOUT,
                           LastErrors());
}

// CastTransport::Delegate interface.
void KeepAliveDelegate::OnError(ChannelError error_state,
                                const LastErrors& last_errors) {
  DCHECK(started_);
  DCHECK(thread_checker_.CalledOnValidThread());
  VLOG(2) << "KeepAlive::OnError";
  inner_delegate_->OnError(error_state, last_errors);
}

void KeepAliveDelegate::OnMessage(const CastMessage& message) {
  DCHECK(started_);
  DCHECK(thread_checker_.CalledOnValidThread());
  VLOG(2) << "KeepAlive::OnMessage : " << message.payload_utf8();

  ResetTimers();

  if (NestedPayloadTypeEquals(kHeartbeatPingType, message)) {
    VLOG(1) << "Received PING.";
    SendKeepAliveMessage(pong_message_, kHeartbeatPongType);
  } else if (NestedPayloadTypeEquals(kHeartbeatPongType, message)) {
    VLOG(1) << "Received PONG.";
  } else {
    // PING and PONG messages are intentionally suppressed from layers above.
    inner_delegate_->OnMessage(message);
  }
}

}  // namespace cast_channel
}  // namespace core_api
}  // namespace extensions

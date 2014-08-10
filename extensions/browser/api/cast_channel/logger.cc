// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/cast_channel/logger.h"

#include "base/strings/string_util.h"
#include "base/time/tick_clock.h"
#include "extensions/browser/api/cast_channel/cast_auth_util.h"

namespace extensions {
namespace core_api {
namespace cast_channel {

using net::IPEndPoint;
using proto::AggregatedSocketEvent;
using proto::EventType;
using proto::Log;
using proto::SocketEvent;

namespace {

const char* kInternalNamespacePrefix = "com.google.cast";

proto::ChallengeReplyErrorType ChannegeReplyErrorToProto(
    AuthResult::ErrorType error_type) {
  switch (error_type) {
    case AuthResult::ERROR_NONE:
      return proto::CHALLENGE_REPLY_ERROR_NONE;
    case AuthResult::ERROR_PEER_CERT_EMPTY:
      return proto::CHALLENGE_REPLY_ERROR_PEER_CERT_EMPTY;
    case AuthResult::ERROR_WRONG_PAYLOAD_TYPE:
      return proto::CHALLENGE_REPLY_ERROR_WRONG_PAYLOAD_TYPE;
    case AuthResult::ERROR_NO_PAYLOAD:
      return proto::CHALLENGE_REPLY_ERROR_NO_PAYLOAD;
    case AuthResult::ERROR_PAYLOAD_PARSING_FAILED:
      return proto::CHALLENGE_REPLY_ERROR_PAYLOAD_PARSING_FAILED;
    case AuthResult::ERROR_MESSAGE_ERROR:
      return proto::CHALLENGE_REPLY_ERROR_MESSAGE_ERROR;
    case AuthResult::ERROR_NO_RESPONSE:
      return proto::CHALLENGE_REPLY_ERROR_NO_RESPONSE;
    case AuthResult::ERROR_FINGERPRINT_NOT_FOUND:
      return proto::CHALLENGE_REPLY_ERROR_FINGERPRINT_NOT_FOUND;
    case AuthResult::ERROR_NSS_CERT_PARSING_FAILED:
      return proto::CHALLENGE_REPLY_ERROR_NSS_CERT_PARSING_FAILED;
    case AuthResult::ERROR_NSS_CERT_NOT_SIGNED_BY_TRUSTED_CA:
      return proto::CHALLENGE_REPLY_ERROR_NSS_CERT_NOT_SIGNED_BY_TRUSTED_CA;
    case AuthResult::ERROR_NSS_CANNOT_EXTRACT_PUBLIC_KEY:
      return proto::CHALLENGE_REPLY_ERROR_NSS_CANNOT_EXTRACT_PUBLIC_KEY;
    case AuthResult::ERROR_NSS_SIGNED_BLOBS_MISMATCH:
      return proto::CHALLENGE_REPLY_ERROR_NSS_SIGNED_BLOBS_MISMATCH;
    default:
      NOTREACHED();
      return proto::CHALLENGE_REPLY_ERROR_NONE;
  }
}

}  // namespace

Logger::AggregatedSocketEventLog::AggregatedSocketEventLog() {
}
Logger::AggregatedSocketEventLog::~AggregatedSocketEventLog() {
}

Logger::Logger(scoped_ptr<base::TickClock> clock,
               base::TimeTicks unix_epoch_time_ticks)
    : clock_(clock.Pass()),
      unix_epoch_time_ticks_(unix_epoch_time_ticks),
      num_evicted_aggregated_socket_events_(0),
      num_evicted_socket_events_(0) {
  DCHECK(clock_);

  // Logger may not be necessarily be created on the IO thread, but logging
  // happens exclusively there.
  thread_checker_.DetachFromThread();
}

Logger::~Logger() {
}

void Logger::LogNewSocketEvent(const CastSocket& cast_socket) {
  DCHECK(thread_checker_.CalledOnValidThread());

  int channel_id = cast_socket.id();
  SocketEvent event = CreateEvent(proto::CAST_SOCKET_CREATED);
  LogSocketEvent(channel_id, event);

  AggregatedSocketEvent& aggregated_socket_event =
      aggregated_socket_events_.find(channel_id)
          ->second->aggregated_socket_event;
  const net::IPAddressNumber& ip = cast_socket.ip_endpoint().address();
  aggregated_socket_event.set_endpoint_id(ip.back());
  aggregated_socket_event.set_channel_auth_type(cast_socket.channel_auth() ==
                                                        CHANNEL_AUTH_TYPE_SSL
                                                    ? proto::SSL
                                                    : proto::SSL_VERIFIED);
}

void Logger::LogSocketEvent(int channel_id, EventType event_type) {
  DCHECK(thread_checker_.CalledOnValidThread());

  LogSocketEventWithDetails(channel_id, event_type, std::string());
}

void Logger::LogSocketEventWithDetails(int channel_id,
                                       EventType event_type,
                                       const std::string& details) {
  DCHECK(thread_checker_.CalledOnValidThread());

  SocketEvent event = CreateEvent(event_type);
  if (!details.empty())
    event.set_details(details);

  LogSocketEvent(channel_id, event);
}

void Logger::LogSocketEventWithRv(int channel_id,
                                  EventType event_type,
                                  int rv) {
  DCHECK(thread_checker_.CalledOnValidThread());

  SocketEvent event = CreateEvent(event_type);
  event.set_return_value(rv);

  LogSocketEvent(channel_id, event);
}

void Logger::LogSocketReadyState(int channel_id, proto::ReadyState new_state) {
  DCHECK(thread_checker_.CalledOnValidThread());

  SocketEvent event = CreateEvent(proto::READY_STATE_CHANGED);
  event.set_ready_state(new_state);

  LogSocketEvent(channel_id, event);
}

void Logger::LogSocketConnectState(int channel_id,
                                   proto::ConnectionState new_state) {
  DCHECK(thread_checker_.CalledOnValidThread());

  SocketEvent event = CreateEvent(proto::CONNECTION_STATE_CHANGED);
  event.set_connection_state(new_state);

  LogSocketEvent(channel_id, event);
}

void Logger::LogSocketReadState(int channel_id, proto::ReadState new_state) {
  DCHECK(thread_checker_.CalledOnValidThread());

  SocketEvent event = CreateEvent(proto::READ_STATE_CHANGED);
  event.set_read_state(new_state);

  LogSocketEvent(channel_id, event);
}

void Logger::LogSocketWriteState(int channel_id, proto::WriteState new_state) {
  DCHECK(thread_checker_.CalledOnValidThread());

  SocketEvent event = CreateEvent(proto::WRITE_STATE_CHANGED);
  event.set_write_state(new_state);

  LogSocketEvent(channel_id, event);
}

void Logger::LogSocketErrorState(int channel_id, proto::ErrorState new_state) {
  DCHECK(thread_checker_.CalledOnValidThread());

  SocketEvent event = CreateEvent(proto::ERROR_STATE_CHANGED);
  event.set_error_state(new_state);

  LogSocketEvent(channel_id, event);
}

void Logger::LogSocketEventForMessage(int channel_id,
                                      EventType event_type,
                                      const std::string& message_namespace,
                                      const std::string& details) {
  DCHECK(thread_checker_.CalledOnValidThread());

  SocketEvent event = CreateEvent(event_type);
  if (StartsWithASCII(message_namespace, kInternalNamespacePrefix, false))
    event.set_message_namespace(message_namespace);
  event.set_details(details);

  LogSocketEvent(channel_id, event);
}

void Logger::LogSocketChallengeReplyEvent(int channel_id,
                                          const AuthResult& auth_result) {
  DCHECK(thread_checker_.CalledOnValidThread());

  SocketEvent event = CreateEvent(proto::AUTH_CHALLENGE_REPLY);
  event.set_challenge_reply_error_type(
      ChannegeReplyErrorToProto(auth_result.error_type));
  if (auth_result.nss_error_code != 0)
    event.set_return_value(auth_result.nss_error_code);

  LogSocketEvent(channel_id, event);
}

SocketEvent Logger::CreateEvent(EventType event_type) {
  SocketEvent event;
  event.set_type(event_type);
  event.set_timestamp_micros(clock_->NowTicks().ToInternalValue() +
                             unix_epoch_time_ticks_.ToInternalValue());
  return event;
}

void Logger::LogSocketEvent(int channel_id, const SocketEvent& socket_event) {
  AggregatedSocketEventLogMap::iterator it =
      aggregated_socket_events_.find(channel_id);
  if (it == aggregated_socket_events_.end()) {
    if (aggregated_socket_events_.size() >= kMaxSocketsToLog) {
      AggregatedSocketEventLogMap::iterator erase_it =
          aggregated_socket_events_.begin();

      num_evicted_aggregated_socket_events_++;
      num_evicted_socket_events_ += erase_it->second->socket_events.size();

      aggregated_socket_events_.erase(erase_it);
    }

    it = aggregated_socket_events_
             .insert(std::make_pair(
                 channel_id, make_linked_ptr(new AggregatedSocketEventLog)))
             .first;
    it->second->aggregated_socket_event.set_id(channel_id);
  }

  std::deque<proto::SocketEvent>& socket_events = it->second->socket_events;
  if (socket_events.size() >= kMaxEventsPerSocket) {
    socket_events.pop_front();
    num_evicted_socket_events_++;
  }

  socket_events.push_back(socket_event);
}

bool Logger::LogToString(std::string* output) const {
  output->clear();

  Log log;
  log.set_num_evicted_aggregated_socket_events(
      num_evicted_aggregated_socket_events_);
  log.set_num_evicted_socket_events(num_evicted_socket_events_);

  for (AggregatedSocketEventLogMap::const_iterator it =
           aggregated_socket_events_.begin();
       it != aggregated_socket_events_.end();
       ++it) {
    AggregatedSocketEvent* new_aggregated_socket_event =
        log.add_aggregated_socket_event();
    new_aggregated_socket_event->CopyFrom(it->second->aggregated_socket_event);

    const std::deque<SocketEvent>& socket_events = it->second->socket_events;
    for (std::deque<SocketEvent>::const_iterator socket_event_it =
             socket_events.begin();
         socket_event_it != socket_events.end();
         ++socket_event_it) {
      SocketEvent* socket_event =
          new_aggregated_socket_event->add_socket_event();
      socket_event->CopyFrom(*socket_event_it);
    }
  }

  return log.SerializeToString(output);
}

void Logger::Reset() {
  aggregated_socket_events_.clear();
  num_evicted_aggregated_socket_events_ = 0;
  num_evicted_socket_events_ = 0;
}

}  // namespace cast_channel
}  // namespace api
}  // namespace extensions

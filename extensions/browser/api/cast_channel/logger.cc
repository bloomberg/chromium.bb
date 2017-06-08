// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/cast_channel/logger.h"

#include <stdint.h>

#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "extensions/browser/api/cast_channel/cast_auth_util.h"
#include "extensions/browser/api/cast_channel/cast_socket.h"
#include "net/base/net_errors.h"

namespace extensions {
namespace api {
namespace cast_channel {

using net::IPEndPoint;
using proto::EventType;
using proto::Log;
using proto::SocketEvent;

namespace {

proto::ChallengeReplyErrorType ChallegeReplyErrorToProto(
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
    case AuthResult::ERROR_CERT_PARSING_FAILED:
      return proto::CHALLENGE_REPLY_ERROR_CERT_PARSING_FAILED;
    case AuthResult::ERROR_CERT_NOT_SIGNED_BY_TRUSTED_CA:
      return proto::CHALLENGE_REPLY_ERROR_CERT_NOT_SIGNED_BY_TRUSTED_CA;
    case AuthResult::ERROR_CANNOT_EXTRACT_PUBLIC_KEY:
      return proto::CHALLENGE_REPLY_ERROR_CANNOT_EXTRACT_PUBLIC_KEY;
    case AuthResult::ERROR_SIGNED_BLOBS_MISMATCH:
      return proto::CHALLENGE_REPLY_ERROR_SIGNED_BLOBS_MISMATCH;
    case AuthResult::ERROR_TLS_CERT_VALIDITY_PERIOD_TOO_LONG:
      return proto::CHALLENGE_REPLY_ERROR_TLS_CERT_VALIDITY_PERIOD_TOO_LONG;
    case AuthResult::ERROR_TLS_CERT_VALID_START_DATE_IN_FUTURE:
      return proto::CHALLENGE_REPLY_ERROR_TLS_CERT_VALID_START_DATE_IN_FUTURE;
    case AuthResult::ERROR_TLS_CERT_EXPIRED:
      return proto::CHALLENGE_REPLY_ERROR_TLS_CERT_EXPIRED;
    case AuthResult::ERROR_CRL_INVALID:
      return proto::CHALLENGE_REPLY_ERROR_CRL_INVALID;
    case AuthResult::ERROR_CERT_REVOKED:
      return proto::CHALLENGE_REPLY_ERROR_CERT_REVOKED;
    default:
      NOTREACHED();
      return proto::CHALLENGE_REPLY_ERROR_NONE;
  }
}

// Propagate any error fields set in |event| to |last_errors|.  If any error
// field in |event| is set, then also set |last_errors->event_type|.
void MaybeSetLastErrors(const SocketEvent& event, LastErrors* last_errors) {
  if (event.has_net_return_value() &&
      event.net_return_value() < net::ERR_IO_PENDING) {
    last_errors->net_return_value = event.net_return_value();
    last_errors->event_type = event.type();
  }
  if (event.has_challenge_reply_error_type()) {
    last_errors->challenge_reply_error_type =
        event.challenge_reply_error_type();
    last_errors->event_type = event.type();
  }
}

}  // namespace

LastErrors::LastErrors()
    : event_type(proto::EVENT_TYPE_UNKNOWN),
      challenge_reply_error_type(proto::CHALLENGE_REPLY_ERROR_NONE),
      net_return_value(net::OK) {}

LastErrors::~LastErrors() {}

Logger::Logger() {
  // Logger may not be necessarily be created on the IO thread, but logging
  // happens exclusively there.
  thread_checker_.DetachFromThread();
}

Logger::~Logger() {
}

void Logger::LogSocketEventWithRv(int channel_id,
                                  EventType event_type,
                                  int rv) {
  DCHECK(thread_checker_.CalledOnValidThread());

  SocketEvent event = CreateEvent(event_type);
  event.set_net_return_value(rv);

  LogSocketEvent(channel_id, event);
}

void Logger::LogSocketChallengeReplyEvent(int channel_id,
                                          const AuthResult& auth_result) {
  DCHECK(thread_checker_.CalledOnValidThread());

  SocketEvent event = CreateEvent(proto::AUTH_CHALLENGE_REPLY);
  event.set_challenge_reply_error_type(
      ChallegeReplyErrorToProto(auth_result.error_type));

  LogSocketEvent(channel_id, event);
}

LastErrors Logger::GetLastErrors(int channel_id) const {
  LastErrorsMap::const_iterator it = last_errors_.find(channel_id);
  if (it != last_errors_.end()) {
    return it->second;
  } else {
    return LastErrors();
  }
}

void Logger::ClearLastErrors(int channel_id) {
  last_errors_.erase(channel_id);
}

SocketEvent Logger::CreateEvent(EventType event_type) {
  SocketEvent event;
  event.set_type(event_type);
  return event;
}

void Logger::LogSocketEvent(int channel_id, const SocketEvent& socket_event) {
  LastErrorsMap::iterator it = last_errors_.find(channel_id);
  if (it == last_errors_.end())
    last_errors_[channel_id] = LastErrors();

  MaybeSetLastErrors(socket_event, &last_errors_[channel_id]);
}

}  // namespace cast_channel
}  // namespace api
}  // namespace extensions

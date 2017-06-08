// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "extensions/browser/api/cast_channel/cast_auth_util.h"
#include "extensions/browser/api/cast_channel/logger.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace api {
namespace cast_channel {

using proto::EventType;
using proto::Log;
using proto::SocketEvent;

TEST(CastChannelLoggerTest, LogLastErrorEvents) {
  scoped_refptr<Logger> logger(new Logger());

  // Net return value is set to an error
  logger->LogSocketEventWithRv(1, EventType::TCP_SOCKET_CONNECT,
                               net::ERR_CONNECTION_FAILED);

  LastErrors last_errors = logger->GetLastErrors(1);
  EXPECT_EQ(last_errors.event_type, proto::TCP_SOCKET_CONNECT);
  EXPECT_EQ(last_errors.net_return_value, net::ERR_CONNECTION_FAILED);

  // Challenge reply error set
  AuthResult auth_result = AuthResult::CreateWithParseError(
      "Some error", AuthResult::ErrorType::ERROR_PEER_CERT_EMPTY);

  logger->LogSocketChallengeReplyEvent(2, auth_result);
  last_errors = logger->GetLastErrors(2);
  EXPECT_EQ(last_errors.event_type, proto::AUTH_CHALLENGE_REPLY);
  EXPECT_EQ(last_errors.challenge_reply_error_type,
            proto::CHALLENGE_REPLY_ERROR_PEER_CERT_EMPTY);

  // Logging a non-error event does not set the LastErrors for the channel.
  logger->LogSocketEventWithRv(3, EventType::TCP_SOCKET_CONNECT, net::OK);
  last_errors = logger->GetLastErrors(3);
  EXPECT_EQ(last_errors.event_type, proto::EVENT_TYPE_UNKNOWN);
  EXPECT_EQ(last_errors.net_return_value, net::OK);
  EXPECT_EQ(last_errors.challenge_reply_error_type,
            proto::CHALLENGE_REPLY_ERROR_NONE);

  // Now log a challenge reply error.  LastErrors will be set.
  auth_result =
      AuthResult("Some error failed", AuthResult::ERROR_WRONG_PAYLOAD_TYPE);
  logger->LogSocketChallengeReplyEvent(3, auth_result);
  last_errors = logger->GetLastErrors(3);
  EXPECT_EQ(last_errors.event_type, proto::AUTH_CHALLENGE_REPLY);
  EXPECT_EQ(last_errors.challenge_reply_error_type,
            proto::CHALLENGE_REPLY_ERROR_WRONG_PAYLOAD_TYPE);

  // Logging a non-error event does not change the LastErrors for the channel.
  logger->LogSocketEventWithRv(3, EventType::TCP_SOCKET_CONNECT, net::OK);
  last_errors = logger->GetLastErrors(3);
  EXPECT_EQ(last_errors.event_type, proto::AUTH_CHALLENGE_REPLY);
  EXPECT_EQ(last_errors.challenge_reply_error_type,
            proto::CHALLENGE_REPLY_ERROR_WRONG_PAYLOAD_TYPE);
}

}  // namespace cast_channel
}  // namespace api
}  // namespace extensions

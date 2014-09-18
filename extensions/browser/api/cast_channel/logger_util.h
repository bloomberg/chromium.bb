// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_CAST_CHANNEL_LOGGER_UTIL_H_
#define EXTENSIONS_BROWSER_API_CAST_CHANNEL_LOGGER_UTIL_H_

#include "extensions/browser/api/cast_channel/logging.pb.h"

namespace extensions {
namespace core_api {
namespace cast_channel {

// TODO(mfoltz): Move the *ToProto functions from cast_socket.cc here.
// CastSocket should not need to know details of the logging protocol message.

// Holds the most recent errors encountered by a CastSocket.
struct LastErrors {
 public:
  LastErrors();
  ~LastErrors();

  // The most recent event that occurred at the time of the error.
  proto::EventType event_type;

  // The most recent ChallengeReplyErrorType logged for the socket.
  proto::ChallengeReplyErrorType challenge_reply_error_type;

  // The most recent net_return_value logged for the socket.
  int net_return_value;

  // The most recent NSS error logged for the socket.
  int nss_error_code;
};

}  // namespace cast_channel
}  // namespace api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_CAST_CHANNEL_LOGGER_UTIL_H_

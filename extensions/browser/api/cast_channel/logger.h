// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_CAST_CHANNEL_LOGGER_H_
#define EXTENSIONS_BROWSER_API_CAST_CHANNEL_LOGGER_H_

#include <stddef.h>

#include <deque>
#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "extensions/common/api/cast_channel/logging.pb.h"


namespace extensions {
namespace api {
namespace cast_channel {

struct AuthResult;

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
};

// Called with events that occur on a Cast Channel and remembers any that
// warrant reporting to the caller in LastErrors.
class Logger : public base::RefCountedThreadSafe<Logger> {
 public:
  Logger();

  // For events that involves socket / crypto operations that returns a value.
  void LogSocketEventWithRv(int channel_id,
                            proto::EventType event_type,
                            int rv);

  // For AUTH_CHALLENGE_REPLY event.
  void LogSocketChallengeReplyEvent(int channel_id,
                                    const AuthResult& auth_result);

  // Returns the last errors logged for |channel_id|.
  LastErrors GetLastErrors(int channel_id) const;

  // Removes a LastErrors entry for |channel_id| if one exists.
  void ClearLastErrors(int channel_id);

 private:
  friend class base::RefCountedThreadSafe<Logger>;
  ~Logger();

  using LastErrorsMap = std::map<int, LastErrors>;

  // Returns a SocketEvent proto with common fields (EventType, timestamp)
  // populated.
  proto::SocketEvent CreateEvent(proto::EventType event_type);

  // Uses |event| associated with |channel_id| to update its LastErrors.
  void LogSocketEvent(int channel_id, const proto::SocketEvent& socket_event);

  LastErrorsMap last_errors_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(Logger);
};
}  // namespace cast_channel
}  // namespace api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_CAST_CHANNEL_LOGGER_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_CAST_CHANNEL_LOGGER_H_
#define EXTENSIONS_BROWSER_API_CAST_CHANNEL_LOGGER_H_

#include <deque>
#include <map>

#include "base/basictypes.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "extensions/browser/api/cast_channel/cast_socket.h"
#include "extensions/browser/api/cast_channel/logger_util.h"
#include "extensions/browser/api/cast_channel/logging.pb.h"
#include "net/base/ip_endpoint.h"

namespace base {
class TickClock;
}

namespace extensions {
namespace core_api {
namespace cast_channel {

struct AuthResult;

static const int kMaxSocketsToLog = 50;
static const int kMaxEventsPerSocket = 2000;

// Logs information of each channel and sockets and exports the log as
// a blob. Logger is done on the IO thread.
class Logger : public base::RefCounted<Logger> {
 public:
  // |clock|: Clock used for generating timestamps for the events. Owned by
  // this class.
  // |unix_epoch_time_ticks|: The TimeTicks that corresponds to Unix epoch.
  Logger(scoped_ptr<base::TickClock> clock,
         base::TimeTicks unix_epoch_time_ticks);

  // For newly created sockets. Will create an event and log a
  // CAST_SOCKET_CREATED event.
  void LogNewSocketEvent(const CastSocket& cast_socket);

  void LogSocketEvent(int channel_id, proto::EventType event_type);
  void LogSocketEventWithDetails(int channel_id,
                                 proto::EventType event_type,
                                 const std::string& details);

  // For events that involves socket / crypto operations that returns a value.
  void LogSocketEventWithRv(int channel_id,
                            proto::EventType event_type,
                            int rv);

  // For *_STATE_CHANGED events.
  void LogSocketReadyState(int channel_id, proto::ReadyState new_state);
  void LogSocketConnectState(int channel_id, proto::ConnectionState new_state);
  void LogSocketReadState(int channel_id, proto::ReadState new_state);
  void LogSocketWriteState(int channel_id, proto::WriteState new_state);
  void LogSocketErrorState(int channel_id, proto::ErrorState new_state);

  // For AUTH_CHALLENGE_REPLY event.
  void LogSocketChallengeReplyEvent(int channel_id,
                                    const AuthResult& auth_result);

  void LogSocketEventForMessage(int channel_id,
                                proto::EventType event_type,
                                const std::string& message_namespace,
                                const std::string& details);

  // Assembles logs collected so far and return it as a serialized Log proto,
  // compressed in gzip format.
  // If serialization or compression failed, returns a NULL pointer.
  // |length|: If successful, assigned with size of compressed content.
  scoped_ptr<char[]> GetLogs(size_t* length) const;

  // Clears the internal map.
  void Reset();

  // Returns the last errors logged for |channel_id|.  If the the logs for
  // |channel_id| are evicted before this is called, returns a LastErrors with
  // no errors.  This may happen if errors are logged and retrieved in different
  // tasks.
  LastErrors GetLastErrors(int channel_id) const;

 private:
  friend class base::RefCounted<Logger>;
  ~Logger();

  struct AggregatedSocketEventLog {
   public:
    AggregatedSocketEventLog();
    ~AggregatedSocketEventLog();

    // Partially constructed AggregatedSocketEvent proto populated by Logger.
    // Contains top level info such as channel ID, IP end point and channel
    // auth type.
    proto::AggregatedSocketEvent aggregated_socket_event;
    // Events to be assigned to the AggregatedSocketEvent proto. Contains the
    // most recent |kMaxEventsPerSocket| entries. The oldest events are
    // evicted as new events are logged.
    std::deque<proto::SocketEvent> socket_events;

    // The most recent errors logged for the socket.
    LastErrors last_errors;
  };

  typedef std::map<int, linked_ptr<AggregatedSocketEventLog> >
      AggregatedSocketEventLogMap;

  // Returns a SocketEvent proto with common fields (EventType, timestamp)
  // populated.
  proto::SocketEvent CreateEvent(proto::EventType event_type);

  // Records |event| associated with |channel_id|.
  // If the internal map is already logging maximum number of sockets and this
  // is a new socket, the socket with the smallest channel id will be discarded.
  // Returns a reference to the AggregatedSocketEvent proto created/modified.
  proto::AggregatedSocketEvent& LogSocketEvent(
      int channel_id,
      const proto::SocketEvent& socket_event);

  scoped_ptr<base::TickClock> clock_;
  AggregatedSocketEventLogMap aggregated_socket_events_;
  base::TimeTicks unix_epoch_time_ticks_;

  // Log proto holding global statistics.
  proto::Log log_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(Logger);
};
}  // namespace cast_channel
}  // namespace api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_CAST_CHANNEL_LOGGER_H_

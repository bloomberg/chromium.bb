// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_CAST_CHANNEL_KEEP_ALIVE_DELEGATE_H_
#define EXTENSIONS_BROWSER_API_CAST_CHANNEL_KEEP_ALIVE_DELEGATE_H_

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "extensions/browser/api/cast_channel/cast_transport.h"
#include "extensions/common/api/cast_channel/cast_channel.pb.h"

namespace extensions {
namespace api {
namespace cast_channel {

class CastSocket;
class Logger;

// Decorator delegate which provides keep-alive functionality.
// Keep-alive messages are handled by this object; all other messages and
// errors are passed to |inner_delegate_|.
class KeepAliveDelegate : public CastTransport::Delegate {
 public:
  // |socket|: The socket to be kept alive.
  // |logger|: The logging object which collects protocol events and error
  //           details.
  // |inner_delegate|: The delegate which processes all non-keep-alive
  //                   messages. This object assumes ownership of
  //                   |inner_delegate|.
  // |ping_interval|: The amount of idle time to wait before sending a PING to
  //                  the remote end.
  // |liveness_timeout|: The amount of idle time to wait before terminating the
  //                     connection.
  KeepAliveDelegate(CastSocket* socket,
                    scoped_refptr<Logger> logger,
                    std::unique_ptr<CastTransport::Delegate> inner_delegate,
                    base::TimeDelta ping_interval,
                    base::TimeDelta liveness_timeout);

  ~KeepAliveDelegate() override;

  // Creates a keep-alive message (e.g. PING or PONG).
  static CastMessage CreateKeepAliveMessage(const char* message_type);

  void SetTimersForTest(std::unique_ptr<base::Timer> injected_ping_timer,
                        std::unique_ptr<base::Timer> injected_liveness_timer);

  // CastTransport::Delegate implementation.
  void Start() override;
  void OnError(ChannelError error_state) override;
  void OnMessage(const CastMessage& message) override;

  static const char kHeartbeatPingType[];
  static const char kHeartbeatPongType[];

 private:
  // Restarts the ping/liveness timeout timers. Called when a message
  // is received from the remote end.
  void ResetTimers();

  // Sends a formatted PING or PONG message to the remote side.
  void SendKeepAliveMessage(const CastMessage& message,
                            const char* message_type);

  // Callback for SendKeepAliveMessage.
  void SendKeepAliveMessageComplete(const char* message_type, int rv);

  // Called when the liveness timer expires, indicating that the remote
  // end has not responded within the |liveness_timeout_| interval.
  void LivenessTimeout();

  // Stops the ping and liveness timers if they are started.
  // To be called after an error.
  void Stop();

  // Indicates that Start() was called.
  bool started_;

  // Socket that is managed by the keep-alive object.
  CastSocket* socket_;

  // Logging object.
  scoped_refptr<Logger> logger_;

  // Delegate object which receives all non-keep alive messages.
  std::unique_ptr<CastTransport::Delegate> inner_delegate_;

  // Amount of idle time to wait before disconnecting.
  base::TimeDelta liveness_timeout_;

  // Amount of idle time to wait before pinging the receiver.
  base::TimeDelta ping_interval_;

  // Fired when |ping_interval_| is exceeded or when triggered by test code.
  std::unique_ptr<base::Timer> ping_timer_;

  // Fired when |liveness_timer_| is exceeded.
  std::unique_ptr<base::Timer> liveness_timer_;

  // The PING message to send over the wire.
  CastMessage ping_message_;

  // The PONG message to send over the wire.
  CastMessage pong_message_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(KeepAliveDelegate);
};

}  // namespace cast_channel
}  // namespace api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_CAST_CHANNEL_KEEP_ALIVE_DELEGATE_H_

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STANDALONE_RECEIVER_CAST_AGENT_H_
#define CAST_STANDALONE_RECEIVER_CAST_AGENT_H_

#include <memory>

#include "cast/common/channel/cast_socket.h"
#include "cast/standalone_receiver/streaming_playback_controller.h"
#include "cast/streaming/environment.h"
#include "cast/streaming/receiver_session.h"
#include "platform/api/scoped_wake_lock.h"
#include "platform/base/error.h"
#include "platform/base/interface_info.h"
#include "platform/impl/task_runner.h"
#include "util/serial_delete_ptr.h"

namespace openscreen {
namespace cast {

class CastAgent : public CastSocket::Client {
 public:
  CastAgent(TaskRunnerImpl* task_runner, InterfaceInfo interface);
  ~CastAgent();

  // Initialization occurs as part of construction, however to actually bind
  // for discovery and listening over TLS, the CastAgent must be started.
  Error Start();
  Error Stop();

  // CastSocket::Client overrides.
  void OnError(CastSocket* socket, Error error) override;
  void OnMessage(CastSocket* socket,
                 ::cast::channel::CastMessage message) override;

 private:
  std::unique_ptr<Environment> environment_;
  TaskRunnerImpl* const task_runner_;
  const InterfaceInfo interface_;

  std::unique_ptr<ScopedWakeLock> wake_lock_;
  std::unique_ptr<StreamingPlaybackController> controller_;
  std::unique_ptr<ReceiverSession> current_session_;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STANDALONE_RECEIVER_CAST_AGENT_H_

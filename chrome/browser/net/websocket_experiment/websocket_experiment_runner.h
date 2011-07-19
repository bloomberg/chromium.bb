// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Run WebSocket live experiment to collect metrics about WebSocket
// availability in the real Internet.
// It tries to open WebSocket connection to websocket-experiment.chromium.org,
// send/receive some static content over the connection, and see how it works
// well.
// It must be started only if the user permits metrics reporting with the
// checkbox in the prefs.
// For more detail what the experiment is, see websocket_experiment_task.h.

#ifndef CHROME_BROWSER_NET_WEBSOCKET_EXPERIMENT_WEBSOCKET_EXPERIMENT_RUNNER_H_
#define CHROME_BROWSER_NET_WEBSOCKET_EXPERIMENT_WEBSOCKET_EXPERIMENT_RUNNER_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/net/websocket_experiment/websocket_experiment_task.h"
#include "net/base/completion_callback.h"

namespace chrome_browser_net_websocket_experiment {

class WebSocketExperimentRunner
    : public base::RefCountedThreadSafe<WebSocketExperimentRunner> {
 public:
  class Config {
   public:
    Config()
        : initial_delay_ms(0),
          next_delay_ms(0) {}

    int64 initial_delay_ms;
    int64 next_delay_ms;
    WebSocketExperimentTask::Config ws_config[6];
  };
  static void Start();
  static void Stop();

  void Run();
  void Cancel();

 private:
  enum State {
    STATE_NONE,
    STATE_IDLE,
    STATE_RUN_WS,
    STATE_RUN_WSS,
    STATE_RUN_WS_NODEFAULT_PORT,
    STATE_RUN_WS_DRAFT75,
    STATE_RUN_WSS_DRAFT75,
    STATE_RUN_WS_NODEFAULT_PORT_DRAFT75,
    NUM_STATES,
  };
  WebSocketExperimentRunner();
  virtual ~WebSocketExperimentRunner();
  friend class base::RefCountedThreadSafe<WebSocketExperimentRunner>;

  void InitConfig();
  void DoLoop();
  void OnTaskCompleted(int result);
  void UpdateTaskResultHistogram(const WebSocketExperimentTask* task);

  Config config_;
  State next_state_;
  State task_state_;
  scoped_ptr<WebSocketExperimentTask> task_;
  net::CompletionCallbackImpl<WebSocketExperimentRunner> task_callback_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketExperimentRunner);
};

}  // namespace chrome_browser_net_websocket_experiment

#endif  // CHROME_BROWSER_NET_WEBSOCKET_EXPERIMENT_WEBSOCKET_EXPERIMENT_RUNNER_H_

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/websocket_experiment/websocket_experiment_runner.h"

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/task.h"
#include "base/string_util.h"
#include "chrome/common/chrome_switches.h"
#include "content/browser/browser_thread.h"
#include "net/base/host_resolver.h"
#include "net/base/net_errors.h"
#include "net/websockets/websocket.h"

namespace chrome_browser_net_websocket_experiment {

static const char *kExperimentHost = "websocket-experiment.chromium.org";
static const int kAlternativePort = 61985;

// Hold reference while experiment is running.
static scoped_refptr<WebSocketExperimentRunner> runner;

/* static */
void WebSocketExperimentRunner::Start() {
  DCHECK(!runner.get());

  // After June 30, 2011 builds, it will always be in default group.
  scoped_refptr<base::FieldTrial> trial(
      new base::FieldTrial(
          "WebSocketExperiment", 1000, "default", 2011, 6, 30));
  int active = trial->AppendGroup("active", 5);  // 0.5% in active group.

  bool run_experiment = (trial->group() == active);
#ifndef NDEBUG
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  std::string experiment_host = command_line.GetSwitchValueASCII(
      switches::kWebSocketLiveExperimentHost);
  if (!experiment_host.empty())
    run_experiment = true;
#else
  run_experiment = false;
#endif
  if (!run_experiment)
    return;

  runner = new WebSocketExperimentRunner;
  runner->Run();
}

/* static */
void WebSocketExperimentRunner::Stop() {
  if (runner.get())
    runner->Cancel();
  runner = NULL;
}

WebSocketExperimentRunner::WebSocketExperimentRunner()
    : next_state_(STATE_NONE),
      task_state_(STATE_NONE),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          task_callback_(this, &WebSocketExperimentRunner::OnTaskCompleted)) {
  WebSocketExperimentTask::InitHistogram();
  InitConfig();
}

WebSocketExperimentRunner::~WebSocketExperimentRunner() {
  DCHECK(!task_.get());
  WebSocketExperimentTask::ReleaseHistogram();
}

void WebSocketExperimentRunner::Run() {
  DCHECK_EQ(next_state_, STATE_NONE);
  next_state_ = STATE_RUN_WS;
  BrowserThread::PostDelayedTask(
      BrowserThread::IO,
      FROM_HERE,
      NewRunnableMethod(this, &WebSocketExperimentRunner::DoLoop),
      config_.initial_delay_ms);
}

void WebSocketExperimentRunner::Cancel() {
  next_state_ = STATE_NONE;
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      NewRunnableMethod(this, &WebSocketExperimentRunner::DoLoop));
}

void WebSocketExperimentRunner::InitConfig() {
  config_.initial_delay_ms = 5 * 60 * 1000;  // 5 mins
  config_.next_delay_ms = 12 * 60 * 60 * 1000;  // 12 hours

  std::string experiment_host = kExperimentHost;
#ifndef NDEBUG
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  std::string experiment_host_override = command_line.GetSwitchValueASCII(
      switches::kWebSocketLiveExperimentHost);
  if (!experiment_host_override.empty()) {
    experiment_host = experiment_host_override;
    config_.initial_delay_ms = 5 * 1000;  // 5 secs.
  }
#endif

  WebSocketExperimentTask::Config* config;
  WebSocketExperimentTask::Config task_config;

  task_config.protocol_version = net::WebSocket::DEFAULT_VERSION;
  config = &config_.ws_config[STATE_RUN_WS - STATE_RUN_WS];
  *config = task_config;
  config->url =
      GURL(StringPrintf("ws://%s/live_exp", experiment_host.c_str()));
  config->ws_location =
      StringPrintf("ws://%s/live_exp", experiment_host.c_str());
  config->http_url =
      GURL(StringPrintf("http://%s/", experiment_host.c_str()));

  config = &config_.ws_config[STATE_RUN_WSS - STATE_RUN_WS];
  *config = task_config;
  config->url =
      GURL(StringPrintf("wss://%s/live_exp", experiment_host.c_str()));
  config->ws_location =
      StringPrintf("wss://%s/live_exp", experiment_host.c_str());
  config->http_url =
      GURL(StringPrintf("https://%s/", experiment_host.c_str()));

  config = &config_.ws_config[STATE_RUN_WS_NODEFAULT_PORT -
                              STATE_RUN_WS];
  *config = task_config;
  config->url =
      GURL(StringPrintf("ws://%s:%d/live_exp",
                        experiment_host.c_str(), kAlternativePort));
  config->ws_location =
      StringPrintf("ws://%s:%d/live_exp",
                   experiment_host.c_str(), kAlternativePort);
  config->http_url =
      GURL(StringPrintf("http://%s:%d/",
                        experiment_host.c_str(), kAlternativePort));

  task_config.protocol_version = net::WebSocket::DRAFT75;
  config = &config_.ws_config[STATE_RUN_WS_DRAFT75 - STATE_RUN_WS];
  *config = task_config;
  config->url =
      GURL(StringPrintf("ws://%s/live_exp", experiment_host.c_str()));
  config->ws_location =
      StringPrintf("ws://%s/live_exp", experiment_host.c_str());
  config->http_url =
      GURL(StringPrintf("http://%s/", experiment_host.c_str()));

  config = &config_.ws_config[STATE_RUN_WSS_DRAFT75 - STATE_RUN_WS];
  *config = task_config;
  config->url =
      GURL(StringPrintf("wss://%s/live_exp", experiment_host.c_str()));
  config->ws_location =
      StringPrintf("wss://%s/live_exp", experiment_host.c_str());
  config->http_url =
      GURL(StringPrintf("https://%s/", experiment_host.c_str()));

  config = &config_.ws_config[STATE_RUN_WS_NODEFAULT_PORT_DRAFT75 -
                              STATE_RUN_WS];
  *config = task_config;
  config->url =
      GURL(StringPrintf("ws://%s:%d/live_exp",
                        experiment_host.c_str(), kAlternativePort));
  config->ws_location =
      StringPrintf("ws://%s:%d/live_exp",
                   experiment_host.c_str(), kAlternativePort);
  config->http_url =
      GURL(StringPrintf("http://%s:%d/",
                        experiment_host.c_str(), kAlternativePort));

}

void WebSocketExperimentRunner::DoLoop() {
  if (next_state_ == STATE_NONE) {
    if (task_.get()) {
      AddRef();  // Release in OnTaskCompleted after Cancelled.
      task_->Cancel();
    }
    return;
  }

  State state = next_state_;
  task_state_ = STATE_NONE;
  next_state_ = STATE_NONE;

  switch (state) {
    case STATE_IDLE:
      task_.reset();
      next_state_ = STATE_RUN_WS;
      BrowserThread::PostDelayedTask(
          BrowserThread::IO,
          FROM_HERE,
          NewRunnableMethod(this, &WebSocketExperimentRunner::DoLoop),
          config_.next_delay_ms);
      break;
    case STATE_RUN_WS:
    case STATE_RUN_WSS:
    case STATE_RUN_WS_NODEFAULT_PORT:
    case STATE_RUN_WS_DRAFT75:
    case STATE_RUN_WSS_DRAFT75:
    case STATE_RUN_WS_NODEFAULT_PORT_DRAFT75:
      task_.reset(new WebSocketExperimentTask(
          config_.ws_config[state - STATE_RUN_WS], &task_callback_));
      task_state_ = state;
      if (static_cast<State>(state + 1) == NUM_STATES)
        next_state_ = STATE_IDLE;
      else
        next_state_ = static_cast<State>(state + 1);
      break;
    default:
      NOTREACHED();
      break;
  }
  if (task_.get())
    task_->Run();
}

void WebSocketExperimentRunner::OnTaskCompleted(int result) {
  if (next_state_ == STATE_NONE) {
    task_.reset();
    // Task is Canceled.
    DVLOG(1) << "WebSocketExperiment Task is canceled.";
    Release();
    return;
  }
  task_->SaveResult();
  task_.reset();

  DoLoop();
}

}  // namespace chrome_browser_net_websocket_experiment

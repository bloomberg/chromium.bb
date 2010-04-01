// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/websocket_experiment/websocket_experiment_runner.h"

#include "base/compiler_specific.h"
#include "base/field_trial.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "chrome/browser/chrome_thread.h"
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

  scoped_refptr<FieldTrial> trial = new FieldTrial("WebSocketExperiment", 1000);
  trial->AppendGroup("_active", 5);  // 0.5% in _active group.

  if (trial->group() == FieldTrial::kNotParticipating)
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
  ChromeThread::PostDelayedTask(
      ChromeThread::IO,
      FROM_HERE,
      NewRunnableMethod(this, &WebSocketExperimentRunner::DoLoop),
      config_.initial_delay_ms);
}

void WebSocketExperimentRunner::Cancel() {
  next_state_ = STATE_NONE;
  ChromeThread::PostTask(
      ChromeThread::IO,
      FROM_HERE,
      NewRunnableMethod(this, &WebSocketExperimentRunner::DoLoop));
}

void WebSocketExperimentRunner::InitConfig() {
  config_.initial_delay_ms = 5 * 60 * 1000;  // 5 mins
  config_.next_delay_ms = 12 * 60 * 60 * 1000;  // 12 hours

  WebSocketExperimentTask::Config task_config;
  task_config.protocol_version = net::WebSocket::DRAFT75;

  config_.ws_config = task_config;
  config_.ws_config.url =
      GURL(StringPrintf("ws://%s/live_exp", kExperimentHost));
  config_.ws_config.ws_location =
      StringPrintf("ws://%s/live_exp", kExperimentHost);
  config_.ws_config.http_url =
      GURL(StringPrintf("http://%s/", kExperimentHost));

  config_.wss_config = task_config;
  config_.wss_config.url =
      GURL(StringPrintf("wss://%s/live_exp", kExperimentHost));
  config_.wss_config.ws_location =
      StringPrintf("wss://%s/live_exp", kExperimentHost);
  config_.wss_config.http_url =
      GURL(StringPrintf("https://%s/", kExperimentHost));

  config_.ws_nondefault_config = task_config;
  config_.ws_nondefault_config.url =
      GURL(StringPrintf("ws://%s:%d/live_exp",
                        kExperimentHost, kAlternativePort));
  config_.ws_nondefault_config.ws_location =
      StringPrintf("ws://%s:%d/live_exp",
                   kExperimentHost, kAlternativePort);
  config_.ws_nondefault_config.http_url =
      GURL(StringPrintf("http://%s:%d/",
                        kExperimentHost, kAlternativePort));
}

void WebSocketExperimentRunner::DoLoop() {
  if (next_state_ == STATE_NONE) {
    if (task_.get()) {
      AddRef();  // Release in OnTaskCompleted.
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
      ChromeThread::PostDelayedTask(
          ChromeThread::IO,
          FROM_HERE,
          NewRunnableMethod(this, &WebSocketExperimentRunner::DoLoop),
          config_.next_delay_ms);
      break;
    case STATE_RUN_WS:
      task_.reset(new WebSocketExperimentTask(config_.ws_config,
                                              &task_callback_));
      task_state_ = STATE_RUN_WS;
      next_state_ = STATE_RUN_WSS;
      break;
    case STATE_RUN_WSS:
      task_.reset(new WebSocketExperimentTask(config_.wss_config,
                                              &task_callback_));
      task_state_ = STATE_RUN_WSS;
      next_state_ = STATE_RUN_WS_NODEFAULT_PORT;
      break;
    case STATE_RUN_WS_NODEFAULT_PORT:
      task_.reset(new WebSocketExperimentTask(config_.ws_nondefault_config,
                                              &task_callback_));
      task_state_ = STATE_RUN_WS_NODEFAULT_PORT;
      next_state_ = STATE_IDLE;
      break;
    default:
      NOTREACHED();
      break;
  }
  if (task_.get())
    task_->Run();
}

void WebSocketExperimentRunner::OnTaskCompleted(int result) {
  if (result == net::ERR_ABORTED) {
    task_.reset();
    // Task is Canceled.
    DLOG(INFO) << "WebSocketExperiment Task is canceled.";
    Release();
    return;
  }
  task_->SaveResult();
  task_.reset();

  DoLoop();
}

}  // namespace chrome_browser_net_websocket_experiment
